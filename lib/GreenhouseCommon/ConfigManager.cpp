#include "ConfigManager.h"

#include <FS.h>
#include <LittleFS.h>

#include "CompileTimeUtils.h"
#include "FileGuard.h"
#include "calibration.h"
#include "utils.h"
#include "Logger.h"

struct ConfigFileHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
};

namespace {
  consteval auto create_hostname() {
    constexpr auto gh_id_str = CompileTimeUtils::to_fixed_string<GH_ID>();
    constexpr auto node_id_str = CompileTimeUtils::to_fixed_string<NODE_ID>();
    return CompileTimeUtils::
        concat<CompileTimeUtils::FixedString("gh-"), gh_id_str, CompileTimeUtils::FixedString("-node-"), node_id_str>();
  }
  constexpr auto COMPILED_HOSTNAME = create_hostname();
}  // namespace

namespace {
// MACRO from platformio.ini
#ifndef FACTORY_API_TOKEN
#define FACTORY_API_TOKEN "TOKEN_NOT_SET"
#endif

  const char DEFAULT_AUTH_TOKEN[] PROGMEM = FACTORY_API_TOKEN;

  // URL Default
  const char DEFAULT_DATA_URL[] PROGMEM = "https://example.com/api/sensor";
  const char DEFAULT_OTA_URL_BASE[] PROGMEM = "https://example.com/api/get-file/";

  // Passwords (Hashed Admin & Plain Portal)
  const char DEFAULT_ADMIN_PASS[] PROGMEM = "YOUR_HASHED_PASSWORD_HERE";
  const char DEFAULT_PORTAL_PASS[] PROGMEM = "admin123";

  // Default Intervals
  constexpr unsigned long DEFAULT_UPLOAD_INTERVAL_MS = 600000UL;  // 10 menit
  constexpr unsigned long DEFAULT_SAMPLE_INTERVAL_MS = 60000UL;   // 1 menit
  constexpr unsigned long DEFAULT_CACHE_INTERVAL_MS = 15000UL;
  constexpr unsigned long DEFAULT_SW_WDT_TIMEOUT_MS = 1800000UL;  // 30 menit

  // File Paths
  const char CONFIG_FILE_PATH[] PROGMEM = "/config.dat";
  const char CONFIG_BACKUP_PATH[] PROGMEM = "/config.bak";
  const char CONFIG_TEMP_PATH[] PROGMEM = "/config.tmp";
  const char WIFI_TEMP_PATH[] PROGMEM = "/wifi_temp.dat";
  const char WIFI_MAIN_PATH[] PROGMEM = "/wifi.dat";
}  // namespace

ConfigManager::ConfigManager() {}

void ConfigManager::registerObserver(IConfigObserver* observer) {
  if (observer) {
    // MEMORY OPTIMIZATION: Pre-allocate to avoid reallocations
    if (m_observers.empty()) {
      m_observers.reserve(4);  // Typical max: ApiClient, SensorManager, etc.
    }
    m_observers.push_back(observer);
  }
}

void ConfigManager::init() {
  (void)load();
}

// === AUTO-RECOVERY FUNCTION ===
void ConfigManager::applyDefaults() {
  LOG_INFO("CONFIG", F("Applying Hardcoded Defaults..."));

  // Restore Strings using sizeof for overflow safety
  char buffer[MAX_URL_LEN];

  // Using %S (uppercase) for PROGMEM strings
  snprintf_P(buffer, sizeof(buffer), PSTR("%S"), DEFAULT_AUTH_TOKEN);
  Utils::copy_string(m_config.AUTH_TOKEN, buffer);

  snprintf_P(buffer, sizeof(buffer), PSTR("%S"), DEFAULT_DATA_URL);
  Utils::copy_string(m_config.DATA_UPLOAD_URL, buffer);

  snprintf_P(buffer, sizeof(buffer), PSTR("%S"), DEFAULT_OTA_URL_BASE);
  Utils::copy_string(m_config.FW_VERSION_CHECK_URL_BASE, buffer);

  char passBuf[MAX_PASS_LEN];
  snprintf_P(passBuf, sizeof(passBuf), PSTR("%S"), DEFAULT_ADMIN_PASS);
  Utils::copy_string(m_config.ADMIN_PASSWORD, passBuf);

  snprintf_P(passBuf, sizeof(passBuf), PSTR("%S"), DEFAULT_PORTAL_PASS);
  Utils::copy_string(m_config.PORTAL_PASSWORD, passBuf);

  // Restore Settings
  m_config.IS_PROVISIONED = true;
  m_config.ALLOW_INSECURE_HTTPS = false;
  m_config.DATA_UPLOAD_INTERVAL_MS = DEFAULT_UPLOAD_INTERVAL_MS;
  m_config.SENSOR_SAMPLE_INTERVAL_MS = DEFAULT_SAMPLE_INTERVAL_MS;
  m_config.CACHE_SEND_INTERVAL_MS = DEFAULT_CACHE_INTERVAL_MS;
  m_config.SOFTWARE_WDT_TIMEOUT_MS = DEFAULT_SW_WDT_TIMEOUT_MS;

  // Restore Calibration (Dari calibration.h)
  m_config.TEMP_OFFSET = CompiledDefaults::TEMP_OFFSET;
  m_config.HUMIDITY_OFFSET = CompiledDefaults::HUMIDITY_OFFSET;
  m_config.LUX_SCALING_FACTOR = CompiledDefaults::LUX_SCALING_FACTOR;

  validateAndSanitize();

  if (save() != ConfigStatus::OK) {
    LOG_ERROR("CONFIG", F("Failed to save defaults!"));
  }
}

ConfigStatus ConfigManager::load() {
  char configFilePath[sizeof(CONFIG_FILE_PATH)];
  strncpy_P(configFilePath, CONFIG_FILE_PATH, sizeof(configFilePath));
  configFilePath[sizeof(configFilePath) - 1] = '\0';

  // 1. Try Main File
  ConfigStatus status = loadFromFile(configFilePath);

  if (status == ConfigStatus::OK) {
    return ConfigStatus::OK;
  }
  
  LOG_WARN("CONFIG", F("Main config corrupt/missing. Checking recovery options..."));

  // 1.5 NEW: Try Pending Temp File (Interrupted Save Recovery)
  // If we crashed exactly after moving Main->Backup but before Temp->Main,
  // the 'new' config is sitting in config.tmp.
  char tempFilePath[sizeof(CONFIG_TEMP_PATH)];
  strncpy_P(tempFilePath, CONFIG_TEMP_PATH, sizeof(tempFilePath));
  tempFilePath[sizeof(tempFilePath) - 1] = '\0';

  status = loadFromFile(tempFilePath);
  if (status == ConfigStatus::OK) {
    LOG_INFO("RECOVERY", F("Recovered config from interrupted save (config.tmp)!"));
    // Finish the job: Rename Temp -> Main
    if (LittleFS.rename(tempFilePath, configFilePath)) {
      LOG_INFO("RECOVERY", F("Promoted config.tmp to config.dat"));
    }
    return ConfigStatus::OK;
  }

  // 2. Try Backup File
  char backupFilePath[sizeof(CONFIG_BACKUP_PATH)];
  strncpy_P(backupFilePath, CONFIG_BACKUP_PATH, sizeof(backupFilePath));
  backupFilePath[sizeof(backupFilePath) - 1] = '\0';

  status = loadFromFile(backupFilePath);
  if (status == ConfigStatus::OK) {
    LOG_INFO("RECOVERY", F("Restored config from Backup!"));
    // Option: Save back to main to keep in sync
    (void)save();
    return ConfigStatus::OK;
  }

  // 3. Fallback to Defaults
  LOG_ERROR("CONFIG", F("All configs unusable. Resetting to Defaults."));
  applyDefaults();
  return ConfigStatus::OK;
}

// Extracted helper
ConfigStatus ConfigManager::loadFromFile(const char* path) {
  if (!LittleFS.exists(path))
    return ConfigStatus::FILE_OPEN_FAILED;

  File f = LittleFS.open(path, "r");
  if (!f)
    return ConfigStatus::FILE_OPEN_FAILED;

  FileGuard configFile(f);
  ConfigFileHeader header;

  if (configFile.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
    return ConfigStatus::FILE_READ_ERROR;
  }

  if (header.magic != CONFIG_MAGIC) {
    return ConfigStatus::MAGIC_MISMATCH;
  }

  // Migration Logic based on Version
  if (header.version == 1) {
    LOG_INFO("CONFIG", F("Detected V1 Config. Migrating to V2..."));

    // Define Legacy Struct Locally
    struct AppConfigV1 {
      std::array<char, MAX_TOKEN_LEN> AUTH_TOKEN;
      std::array<char, MAX_URL_LEN> DATA_UPLOAD_URL;
      std::array<char, MAX_URL_LEN> FW_VERSION_CHECK_URL_BASE;
      std::array<char, MAX_PASS_LEN> ADMIN_PASSWORD;
      std::array<char, MAX_PASS_LEN> PORTAL_PASSWORD;
      bool IS_PROVISIONED;
      unsigned long DATA_UPLOAD_INTERVAL_MS;
      unsigned long SENSOR_SAMPLE_INTERVAL_MS;
      unsigned long CACHE_SEND_INTERVAL_MS;
      unsigned long SOFTWARE_WDT_TIMEOUT_MS;
      float TEMP_OFFSET;
      float HUMIDITY_OFFSET;
      float LUX_SCALING_FACTOR;
    } config_v1;

    if (configFile.read((uint8_t*)&config_v1, sizeof(AppConfigV1)) != sizeof(AppConfigV1)) {
      return ConfigStatus::FILE_READ_ERROR;
    }

    // Map V1 -> V2 (m_config)
    m_config.AUTH_TOKEN = config_v1.AUTH_TOKEN;
    m_config.DATA_UPLOAD_URL = config_v1.DATA_UPLOAD_URL;
    m_config.FW_VERSION_CHECK_URL_BASE = config_v1.FW_VERSION_CHECK_URL_BASE;
    m_config.ADMIN_PASSWORD = config_v1.ADMIN_PASSWORD;
    m_config.PORTAL_PASSWORD = config_v1.PORTAL_PASSWORD;
    m_config.IS_PROVISIONED = config_v1.IS_PROVISIONED;
    m_config.ALLOW_INSECURE_HTTPS = false;  // Default for Migration
    m_config.DATA_UPLOAD_INTERVAL_MS = config_v1.DATA_UPLOAD_INTERVAL_MS;
    m_config.SENSOR_SAMPLE_INTERVAL_MS = config_v1.SENSOR_SAMPLE_INTERVAL_MS;
    m_config.CACHE_SEND_INTERVAL_MS = config_v1.CACHE_SEND_INTERVAL_MS;
    m_config.SOFTWARE_WDT_TIMEOUT_MS = config_v1.SOFTWARE_WDT_TIMEOUT_MS;
    m_config.TEMP_OFFSET = config_v1.TEMP_OFFSET;
    m_config.HUMIDITY_OFFSET = config_v1.HUMIDITY_OFFSET;
    m_config.LUX_SCALING_FACTOR = config_v1.LUX_SCALING_FACTOR;

    // Decrypt Legacy Data (It was stored encrypted in V1 too)
    Utils::scramble_data(m_config.AUTH_TOKEN);
    Utils::scramble_data(m_config.PORTAL_PASSWORD);

    validateAndSanitize();

    // Save immediately to upgrade file structure on disk
    (void)save();
    return ConfigStatus::OK;
  }

  // Version 2+ Logic
  if (configFile.read((uint8_t*)&m_config, sizeof(AppConfig)) != sizeof(AppConfig)) {
    return ConfigStatus::FILE_READ_ERROR;
  }

  // Config is valid structurally. Decrypt now.
  Utils::scramble_data(m_config.AUTH_TOKEN);
  Utils::scramble_data(m_config.PORTAL_PASSWORD);

  validateAndSanitize();
  return ConfigStatus::OK;
}

ConfigStatus ConfigManager::save() {
  char configFilePath[sizeof(CONFIG_FILE_PATH)];
  strncpy_P(configFilePath, CONFIG_FILE_PATH, sizeof(configFilePath));
  configFilePath[sizeof(configFilePath) - 1] = '\0';
  char tempFilePath[sizeof(CONFIG_TEMP_PATH)];
  strncpy_P(tempFilePath, CONFIG_TEMP_PATH, sizeof(tempFilePath));
  tempFilePath[sizeof(tempFilePath) - 1] = '\0';

  File f = LittleFS.open(tempFilePath, "w");
  if (!f)
    return ConfigStatus::FILE_OPEN_FAILED;

  {
    FileGuard configFile(f);
    ConfigFileHeader header;
    header.magic = CONFIG_MAGIC;
    header.version = CONFIG_VERSION;
    header.reserved = 0;

    // Copy & Scramble
    AppConfig temp_config = m_config;
    Utils::scramble_data(temp_config.AUTH_TOKEN);
    Utils::scramble_data(temp_config.PORTAL_PASSWORD);

    configFile.write((const uint8_t*)&header, sizeof(header));
    size_t written = configFile.write((const uint8_t*)&temp_config, sizeof(AppConfig));

    if (written != sizeof(AppConfig)) {
      return ConfigStatus::FILE_WRITE_FAILED;
    }
  }

  // Rotation: Main -> Backup, Temp -> Main
  // Delete old backup (if exists)
  char backupFilePath[sizeof(CONFIG_BACKUP_PATH)];
  strncpy_P(backupFilePath, CONFIG_BACKUP_PATH, sizeof(backupFilePath));
  backupFilePath[sizeof(backupFilePath) - 1] = '\0';

  if (LittleFS.exists(backupFilePath)) {
    LittleFS.remove(backupFilePath);
  }

  // Rename config.dat -> config.bak
  if (LittleFS.exists(configFilePath)) {
    LittleFS.rename(configFilePath, backupFilePath);
  }

  // Rename config.tmp -> config.dat
  if (LittleFS.rename(tempFilePath, configFilePath)) {
    notifyObservers();
    return ConfigStatus::OK;
  }

  // If rename fails, try to restore backup to main (optional, but just leave backup as .bak)
  return ConfigStatus::FILE_WRITE_FAILED;
}

// --- Getters / Setters ---

void ConfigManager::notifyObservers() {
  for (auto observer : m_observers)
    observer->onConfigUpdated();
}
const AppConfig& ConfigManager::getConfig() const {
  return m_config;
}
void ConfigManager::setAuthToken(std::string_view token) {
  Utils::copy_string(m_config.AUTH_TOKEN, token);
}
void ConfigManager::setPortalPassword(std::string_view password) {
  Utils::copy_string(m_config.PORTAL_PASSWORD, password);
}
void ConfigManager::setTimingConfig(const AppConfig& tempConfig) {
  m_config.DATA_UPLOAD_INTERVAL_MS = tempConfig.DATA_UPLOAD_INTERVAL_MS;
  m_config.SENSOR_SAMPLE_INTERVAL_MS = tempConfig.SENSOR_SAMPLE_INTERVAL_MS;
  m_config.CACHE_SEND_INTERVAL_MS = tempConfig.CACHE_SEND_INTERVAL_MS;
  m_config.SOFTWARE_WDT_TIMEOUT_MS = tempConfig.SOFTWARE_WDT_TIMEOUT_MS;
  validateAndSanitize();
}
void ConfigManager::setProvisioned(bool isProvisioned) {
  m_config.IS_PROVISIONED = isProvisioned;
}
void ConfigManager::setCalibration(float temp_offset, float humidity_offset, float lux_factor) {
  m_config.TEMP_OFFSET = temp_offset;
  m_config.HUMIDITY_OFFSET = humidity_offset;
  m_config.LUX_SCALING_FACTOR = lux_factor;
}
String ConfigManager::getHostname() const {
  return String(COMPILED_HOSTNAME.c_str());
}
bool ConfigManager::factoryReset() {
  LOG_WARN("CONFIG", F("Formatting..."));
  return LittleFS.format();
}
void ConfigManager::validateAndSanitize() {
  // Timing validation
  m_config.DATA_UPLOAD_INTERVAL_MS = max(m_config.DATA_UPLOAD_INTERVAL_MS, 5000UL);
  m_config.SENSOR_SAMPLE_INTERVAL_MS = max(m_config.SENSOR_SAMPLE_INTERVAL_MS, 1000UL);
  m_config.CACHE_SEND_INTERVAL_MS = max(m_config.CACHE_SEND_INTERVAL_MS, 1000UL);
  m_config.SOFTWARE_WDT_TIMEOUT_MS = max(m_config.SOFTWARE_WDT_TIMEOUT_MS, 60000UL);

  // HARDENING: URL Protocol Validation - Force HTTPS unless explicitly allowed
  if (!m_config.ALLOW_INSECURE_HTTPS) {
    // Check DATA_UPLOAD_URL starts with https://
    if (strncmp(m_config.DATA_UPLOAD_URL.data(), "https://", 8) != 0) {
      LOG_WARN("SECURITY", F("DATA_UPLOAD_URL must use HTTPS. Resetting to default."));
      char buffer[MAX_URL_LEN];
      snprintf_P(buffer, sizeof(buffer), PSTR("%S"), PSTR("https://example.com/api/sensor"));
      Utils::copy_string(m_config.DATA_UPLOAD_URL, buffer);
    }
    // Check OTA URL starts with https://
    if (strncmp(m_config.FW_VERSION_CHECK_URL_BASE.data(), "https://", 8) != 0) {
      LOG_WARN("SECURITY", F("FW_VERSION_CHECK_URL must use HTTPS. Resetting to default."));
      char buffer[MAX_URL_LEN];
      snprintf_P(buffer, sizeof(buffer), PSTR("%S"), PSTR("https://example.com/api/get-file/"));
      Utils::copy_string(m_config.FW_VERSION_CHECK_URL_BASE, buffer);
    }
  }

  // HARDENING: Token Length Validation - Reject suspiciously short/long tokens
  size_t tokenLen = strnlen(m_config.AUTH_TOKEN.data(), MAX_TOKEN_LEN);
  if (tokenLen < 10 || tokenLen >= MAX_TOKEN_LEN - 1) {
    LOG_WARN("SECURITY", F("AUTH_TOKEN invalid length (%zu). Check config."), tokenLen);
    // Don't reset - just warn. User might be mid-provisioning.
  }
}

// --- WiFi Helpers ---

bool ConfigManager::wifiCredentialsExist() {
  char path[sizeof(WIFI_MAIN_PATH)];
  strncpy_P(path, WIFI_MAIN_PATH, sizeof(path));
  path[sizeof(path) - 1] = '\0';
  return LittleFS.exists(path);
}

bool ConfigManager::tempWifiCredentialsExist() {
  char path[sizeof(WIFI_TEMP_PATH)];
  strcpy_P(path, WIFI_TEMP_PATH);
  return LittleFS.exists(path);
}

void ConfigManager::getWifiCredentials(std::span<char> ssid_buf, std::span<char> pass_buf) {
  ssid_buf[0] = '\0';
  pass_buf[0] = '\0';
  char path[sizeof(WIFI_MAIN_PATH)];
  strncpy_P(path, WIFI_MAIN_PATH, sizeof(path));
  path[sizeof(path) - 1] = '\0';
  File f = LittleFS.open(path, "r");
  if (!f)
    return;
  WifiCredentials creds;
  if (f.read((uint8_t*)&creds, sizeof(creds)) == sizeof(creds)) {
    Utils::copy_string(ssid_buf, creds.ssid);
    Utils::copy_string(pass_buf, creds.pass);
    Utils::scramble_data(pass_buf);
  }
  f.close();
  Utils::trim_inplace(ssid_buf);
}

void ConfigManager::loadTempWifiCredentials(std::span<char> ssid_buf, std::span<char> pass_buf) {
  ssid_buf[0] = '\0';
  pass_buf[0] = '\0';
  char path[sizeof(WIFI_TEMP_PATH)];
  strcpy_P(path, WIFI_TEMP_PATH);
  File f = LittleFS.open(path, "r");
  if (!f)
    return;
  WifiCredentials creds;
  if (f.read((uint8_t*)&creds, sizeof(creds)) == sizeof(creds)) {
    Utils::copy_string(ssid_buf, creds.ssid);
    Utils::copy_string(pass_buf, creds.pass);
    Utils::scramble_data(pass_buf);
  }
  f.close();
  Utils::trim_inplace(ssid_buf);
}

bool ConfigManager::saveTempWifiCredentials(std::string_view ssid, std::string_view password) {
  WifiCredentials creds;
  memset(&creds, 0, sizeof(creds));
  Utils::copy_string(std::span{creds.ssid}, ssid);
  Utils::copy_string(std::span{creds.pass}, password);
  Utils::scramble_data(std::span{creds.pass});
  char path[sizeof(WIFI_TEMP_PATH)];
  strcpy_P(path, WIFI_TEMP_PATH);
  File f = LittleFS.open(path, "w");
  if (!f)
    return false;
  size_t written = f.write((const uint8_t*)&creds, sizeof(creds));
  f.close();
  return written == sizeof(creds);
}

bool ConfigManager::promoteTempWifiCredentials() {
  char temp_path[sizeof(WIFI_TEMP_PATH)];
  strncpy_P(temp_path, WIFI_TEMP_PATH, sizeof(temp_path));
  temp_path[sizeof(temp_path) - 1] = '\0';
  char main_path[sizeof(WIFI_MAIN_PATH)];
  strncpy_P(main_path, WIFI_MAIN_PATH, sizeof(main_path));
  main_path[sizeof(main_path) - 1] = '\0';
  if (!LittleFS.exists(temp_path))
    return false;
  if (LittleFS.exists(main_path))
    LittleFS.remove(main_path);
  return LittleFS.rename(temp_path, main_path);
}

void ConfigManager::clearTempWifiCredentials() {
  char path[sizeof(WIFI_TEMP_PATH)];
  strcpy_P(path, WIFI_TEMP_PATH);
  LittleFS.remove(path);
}