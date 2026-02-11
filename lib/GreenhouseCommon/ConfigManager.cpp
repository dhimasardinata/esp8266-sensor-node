#include "ConfigManager.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#include <algorithm>
#include <new>

#include "CompileTimeUtils.h"
// #include "FileGuard.h"
#include "Logger.h"
#include "calibration.h"
#include "constants.h"
#include "utils.h"

struct ConfigFileHeaderV2 {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
};

struct ConfigFileHeaderV3 {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t crc;
};

// On-disk config layout (kept stable for backward compatibility).
struct StoredConfig {
  uint32_t DATA_UPLOAD_INTERVAL_MS;
  uint32_t SENSOR_SAMPLE_INTERVAL_MS;
  uint32_t CACHE_SEND_INTERVAL_MS;
  uint32_t SOFTWARE_WDT_TIMEOUT_MS;

  float TEMP_OFFSET;
  float HUMIDITY_OFFSET;
  float LUX_SCALING_FACTOR;

  std::array<char, MAX_TOKEN_LEN> AUTH_TOKEN;
  std::array<char, MAX_URL_LEN> DATA_UPLOAD_URL;
  std::array<char, MAX_URL_LEN> FW_VERSION_CHECK_URL_BASE;
  std::array<char, MAX_PASS_LEN> ADMIN_PASSWORD;
  std::array<char, MAX_PASS_LEN> PORTAL_PASSWORD;

  ConfigFlags flags;
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
#define FACTORY_API_TOKEN "REDACTED"
#endif

  const char DEFAULT_AUTH_TOKEN[] PROGMEM = REDACTED

  // Default Intervals.
  constexpr unsigned long DEFAULT_UPLOAD_INTERVAL_MS = 600000UL;  // 10 minutes
  constexpr unsigned long DEFAULT_SAMPLE_INTERVAL_MS = 60000UL;   // 1 minute
  constexpr unsigned long DEFAULT_CACHE_INTERVAL_MS = 15000UL;
  constexpr unsigned long DEFAULT_SW_WDT_TIMEOUT_MS = 1800000UL;  // 30 minutes

  // File Paths
  const char CONFIG_FILE_PATH[] PROGMEM = "/config.dat";
  const char CONFIG_BACKUP_PATH[] PROGMEM = "/config.bak";
  const char CONFIG_TEMP_PATH[] PROGMEM = "/config.tmp";
  const char WIFI_TEMP_PATH[] PROGMEM = REDACTED
  const char WIFI_MAIN_PATH[] PROGMEM = REDACTED

  uint32_t crc32_ieee(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t crc = ~0u;
    while (len--) {
      crc ^= *p++;
      for (int i = 0; i < 8; ++i) {
        crc = (crc >> 1) ^ (0xEDB88320u & (-(static_cast<int32_t>(crc & 1))));
      }
    }
    return ~crc;
  }

  bool isValidHttpsUrl(const char* url) {
    size_t len = strnlen(url, MAX_URL_LEN + 1);
    return len >= 12 && len < MAX_URL_LEN && strncmp(url, "https://", 8) == 0;
  }

  bool formatLittleFsSafe() {
    ESP.wdtDisable();
    bool ok = LittleFS.format();
    ESP.wdtEnable(8000);
    return ok;
  }

  void sanitizeString(std::span<char> s) {
    for (char& c : s) {
      if (c != '\0' && (c < 32 || c > 126))
        c = '?';
    }
  }

}  // namespace

ConfigManager::ConfigManager() {}

void ConfigManager::registerObserver(IConfigObserver* observer) {
  if (observer && m_observerCount < m_observers.size()) {
    m_observers[m_observerCount++] = observer;
  }
}

void ConfigManager::init() {
  (void)load();
}

// === AUTO-RECOVERY FUNCTION ===
void ConfigManager::applyDefaults() {
  LOG_INFO("CONFIG", F("Applying Factory Defaults (constexpr)..."));

  // OPTIMIZATION: Single assignment from constexpr struct
  // Replaces ~10 snprintf_P calls and multiple copy operations
  m_config = FactoryDefaults::CONFIG;

  if (!m_strings) {
    std::unique_ptr<AppConfigStrings> strings(new (std::nothrow) AppConfigStrings(FactoryDefaults::STRINGS));
    if (!strings) {
      LOG_ERROR("CONFIG", F("Failed to allocate config strings"));
      return;
    }
    m_strings.swap(strings);
  } else {
    *m_strings = FactoryDefaults::STRINGS;
  }

  // Override token from platformio.ini build flag (not hardcoded in header)
  // DEFAULT_AUTH_TOKEN contains FACTORY_API_TOKEN from platformio.ini
  strncpy_P(m_strings->AUTH_TOKEN.data(), DEFAULT_AUTH_TOKEN, MAX_TOKEN_LEN - 1);
  m_strings->AUTH_TOKEN[MAX_TOKEN_LEN - 1] = REDACTED

  validateAndSanitize();
  validateAndSanitizeStrings(*m_strings);

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

  // Attempt to recover from an interrupted save (temp file).
  // If a crash occurred during rotation (Main -> Backup, but before Temp -> Main),
  // valid configuration may exist in config.tmp.
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

  File configFile = f;
  ConfigFileHeaderV2 header_v2;

  if (configFile.read((uint8_t*)&header_v2, sizeof(header_v2)) != sizeof(header_v2)) {
    configFile.close();
    return ConfigStatus::FILE_READ_ERROR;
  }

  if (header_v2.magic != CONFIG_MAGIC) {
    configFile.close();
    return ConfigStatus::MAGIC_MISMATCH;
  }

  // Migration Logic based on Version
  if (header_v2.version == 1) {
    LOG_INFO("CONFIG", F("Detected V1 Config. Migrating to V2..."));

    // Define Legacy Struct for Migration.
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
      configFile.close();
      return ConfigStatus::FILE_READ_ERROR;
    }

    // Map V1 -> runtime config
    m_config.set_provisioned(config_v1.IS_PROVISIONED);
    m_config.set_insecure(false);  // Default for migrated configs.
    m_config.DATA_UPLOAD_INTERVAL_MS = config_v1.DATA_UPLOAD_INTERVAL_MS;
    m_config.SENSOR_SAMPLE_INTERVAL_MS = config_v1.SENSOR_SAMPLE_INTERVAL_MS;
    m_config.CACHE_SEND_INTERVAL_MS = config_v1.CACHE_SEND_INTERVAL_MS;
    m_config.SOFTWARE_WDT_TIMEOUT_MS = config_v1.SOFTWARE_WDT_TIMEOUT_MS;
    m_config.TEMP_OFFSET = config_v1.TEMP_OFFSET;
    m_config.HUMIDITY_OFFSET = config_v1.HUMIDITY_OFFSET;
    m_config.LUX_SCALING_FACTOR = config_v1.LUX_SCALING_FACTOR;

    AppConfigStrings strings{};
    strings.AUTH_TOKEN = REDACTED
    strings.DATA_UPLOAD_URL = config_v1.DATA_UPLOAD_URL;
    strings.FW_VERSION_CHECK_URL_BASE = config_v1.FW_VERSION_CHECK_URL_BASE;
    strings.ADMIN_PASSWORD = REDACTED
    strings.PORTAL_PASSWORD = REDACTED

    // Decrypt Legacy Data (stored encrypted in V1).
    Utils::scramble_data(strings.AUTH_TOKEN);
    Utils::scramble_data(strings.PORTAL_PASSWORD);

    validateAndSanitize();
    (void)validateAndSanitizeStrings(strings);

    if (!m_strings) {
      std::unique_ptr<AppConfigStrings> buf(new (std::nothrow) AppConfigStrings(strings));
      if (buf) {
        m_strings.swap(buf);
      }
    } else {
      *m_strings = strings;
    }

    // Save immediately to upgrade file structure on disk
    configFile.close();
    (void)save();
    return ConfigStatus::OK;
  }

  uint32_t expected_crc = 0;
  if (header_v2.version >= 3) {
    ConfigFileHeaderV3 header_v3;
    header_v3.magic = header_v2.magic;
    header_v3.version = header_v2.version;
    header_v3.reserved = header_v2.reserved;
    if (configFile.read((uint8_t*)&header_v3.crc, sizeof(header_v3.crc)) != sizeof(header_v3.crc)) {
      configFile.close();
      return ConfigStatus::FILE_READ_ERROR;
    }
    expected_crc = header_v3.crc;
  }

  // Version 2+ Logic (read on-disk layout into temp)
  StoredConfig stored{};
  if (configFile.read((uint8_t*)&stored, sizeof(StoredConfig)) != sizeof(StoredConfig)) {
    configFile.close();
    return ConfigStatus::FILE_READ_ERROR;
  }
  configFile.close();

  if (header_v2.version >= 3) {
    uint32_t actual_crc = crc32_ieee(&stored, sizeof(StoredConfig));
    if (actual_crc != expected_crc) {
      return ConfigStatus::FILE_READ_ERROR;
    }
  }

  // Copy numeric + flags to runtime config.
  m_config.DATA_UPLOAD_INTERVAL_MS = stored.DATA_UPLOAD_INTERVAL_MS;
  m_config.SENSOR_SAMPLE_INTERVAL_MS = stored.SENSOR_SAMPLE_INTERVAL_MS;
  m_config.CACHE_SEND_INTERVAL_MS = stored.CACHE_SEND_INTERVAL_MS;
  m_config.SOFTWARE_WDT_TIMEOUT_MS = stored.SOFTWARE_WDT_TIMEOUT_MS;
  m_config.TEMP_OFFSET = stored.TEMP_OFFSET;
  m_config.HUMIDITY_OFFSET = stored.HUMIDITY_OFFSET;
  m_config.LUX_SCALING_FACTOR = stored.LUX_SCALING_FACTOR;
  m_config.flags = stored.flags;

  AppConfigStrings strings{};
  strings.AUTH_TOKEN = REDACTED
  strings.DATA_UPLOAD_URL = stored.DATA_UPLOAD_URL;
  strings.FW_VERSION_CHECK_URL_BASE = stored.FW_VERSION_CHECK_URL_BASE;
  strings.ADMIN_PASSWORD = REDACTED
  strings.PORTAL_PASSWORD = REDACTED

  // Config is valid structurally. Decrypt now.
  Utils::scramble_data(strings.AUTH_TOKEN);
  Utils::scramble_data(strings.PORTAL_PASSWORD);

  validateAndSanitize();
  const bool stringsChanged = validateAndSanitizeStrings(strings);

  if (header_v2.version < CONFIG_VERSION || stringsChanged) {
    if (!m_strings) {
      std::unique_ptr<AppConfigStrings> buf(new (std::nothrow) AppConfigStrings(strings));
      if (buf) {
        m_strings.swap(buf);
      }
    } else {
      *m_strings = strings;
    }
    (void)save();  // migrate to latest format (adds CRC)
  } else {
    m_strings.reset();
  }
  return ConfigStatus::OK;
}

ConfigStatus ConfigManager::save() {
  if (!ensureStringsLoaded()) {
    return ConfigStatus::FILE_WRITE_FAILED;
  }
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
    File configFile = f;
    ConfigFileHeaderV3 header;
    header.magic = CONFIG_MAGIC;
    header.version = CONFIG_VERSION;
    header.reserved = 0;

    // Copy & Scramble (on-disk layout)
    StoredConfig temp_config{};
    temp_config.DATA_UPLOAD_INTERVAL_MS = m_config.DATA_UPLOAD_INTERVAL_MS;
    temp_config.SENSOR_SAMPLE_INTERVAL_MS = m_config.SENSOR_SAMPLE_INTERVAL_MS;
    temp_config.CACHE_SEND_INTERVAL_MS = m_config.CACHE_SEND_INTERVAL_MS;
    temp_config.SOFTWARE_WDT_TIMEOUT_MS = m_config.SOFTWARE_WDT_TIMEOUT_MS;
    temp_config.TEMP_OFFSET = m_config.TEMP_OFFSET;
    temp_config.HUMIDITY_OFFSET = m_config.HUMIDITY_OFFSET;
    temp_config.LUX_SCALING_FACTOR = m_config.LUX_SCALING_FACTOR;
    temp_config.flags = m_config.flags;
    temp_config.AUTH_TOKEN = REDACTED
    temp_config.DATA_UPLOAD_URL = m_strings->DATA_UPLOAD_URL;
    temp_config.FW_VERSION_CHECK_URL_BASE = m_strings->FW_VERSION_CHECK_URL_BASE;
    temp_config.ADMIN_PASSWORD = REDACTED
    temp_config.PORTAL_PASSWORD = REDACTED

    Utils::scramble_data(temp_config.AUTH_TOKEN);
    Utils::scramble_data(temp_config.PORTAL_PASSWORD);

    header.crc = crc32_ieee(&temp_config, sizeof(StoredConfig));
    configFile.write((const uint8_t*)&header, sizeof(header));
    size_t written = configFile.write((const uint8_t*)&temp_config, sizeof(StoredConfig));

    configFile.close();

    if (written != sizeof(StoredConfig)) {
      return ConfigStatus::FILE_WRITE_FAILED;
    }
  }

  // Rotation: REDACTED
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
  for (size_t i = 0; i < m_observerCount; ++i)
    m_observers[i]->onConfigUpdated();
}
const AppConfig& ConfigManager::getConfig() const {
  return m_config;
}

bool ConfigManager::ensureStringsLoaded() {
  if (m_strings) {
    return true;
  }
  std::unique_ptr<AppConfigStrings> strings(new (std::nothrow) AppConfigStrings());
  if (!strings) {
    return false;
  }

  bool loaded = false;
  char configFilePath[sizeof(CONFIG_FILE_PATH)];
  strncpy_P(configFilePath, CONFIG_FILE_PATH, sizeof(configFilePath));
  configFilePath[sizeof(configFilePath) - 1] = '\0';
  if (LittleFS.exists(configFilePath)) {
    File f = LittleFS.open(configFilePath, "r");
    if (f) {
      ConfigFileHeaderV2 header_v2{};
      if (f.read((uint8_t*)&header_v2, sizeof(header_v2)) == sizeof(header_v2) &&
          header_v2.magic == CONFIG_MAGIC) {
        uint32_t expected_crc = 0;
        if (header_v2.version >= 3) {
          ConfigFileHeaderV3 header_v3{};
          header_v3.magic = header_v2.magic;
          header_v3.version = header_v2.version;
          header_v3.reserved = header_v2.reserved;
          if (f.read((uint8_t*)&header_v3.crc, sizeof(header_v3.crc)) == sizeof(header_v3.crc)) {
            expected_crc = header_v3.crc;
          }
        }
        StoredConfig stored{};
        if (f.read((uint8_t*)&stored, sizeof(StoredConfig)) == sizeof(StoredConfig)) {
          if (header_v2.version >= 3) {
            uint32_t actual_crc = crc32_ieee(&stored, sizeof(StoredConfig));
            if (actual_crc != expected_crc) {
              stored.AUTH_TOKEN = REDACTED
              stored.DATA_UPLOAD_URL = FactoryDefaults::STRINGS.DATA_UPLOAD_URL;
              stored.FW_VERSION_CHECK_URL_BASE = FactoryDefaults::STRINGS.FW_VERSION_CHECK_URL_BASE;
              stored.ADMIN_PASSWORD = REDACTED
              stored.PORTAL_PASSWORD = REDACTED
            }
          }
          strings->AUTH_TOKEN = REDACTED
          strings->DATA_UPLOAD_URL = stored.DATA_UPLOAD_URL;
          strings->FW_VERSION_CHECK_URL_BASE = stored.FW_VERSION_CHECK_URL_BASE;
          strings->ADMIN_PASSWORD = REDACTED
          strings->PORTAL_PASSWORD = REDACTED
          Utils::scramble_data(strings->AUTH_TOKEN);
          Utils::scramble_data(strings->PORTAL_PASSWORD);
          (void)validateAndSanitizeStrings(*strings);
          loaded = true;
        }
      }
      f.close();
    }
  }

  if (!loaded) {
    *strings = FactoryDefaults::STRINGS;
    strncpy_P(strings->AUTH_TOKEN.data(), DEFAULT_AUTH_TOKEN, MAX_TOKEN_LEN - 1);
    strings->AUTH_TOKEN[MAX_TOKEN_LEN - 1] = REDACTED
    (void)validateAndSanitizeStrings(*strings);
  }

  m_strings.swap(strings);
  return true;
}

const AppConfigStrings& ConfigManager::getStrings() {
  (void)ensureStringsLoaded();
  return *m_strings;
}
void ConfigManager::releaseStrings() {
  m_strings.reset();
}
const char* ConfigManager::getAuthToken() {
  (void)ensureStringsLoaded();
  return m_strings ? m_strings->AUTH_TOKEN.data() : REDACTED
}
const char* ConfigManager::getDataUploadUrl() {
  (void)ensureStringsLoaded();
  return m_strings ? m_strings->DATA_UPLOAD_URL.data() : "";
}
const char* ConfigManager::getOtaUrlBase() {
  (void)ensureStringsLoaded();
  return m_strings ? m_strings->FW_VERSION_CHECK_URL_BASE.data() : "";
}
const char* ConfigManager::getAdminPassword() {
  (void)ensureStringsLoaded();
  return m_strings ? m_strings->ADMIN_PASSWORD.data() : REDACTED
}
const char* ConfigManager::getPortalPassword() {
  (void)ensureStringsLoaded();
  return m_strings ? m_strings->PORTAL_PASSWORD.data() : REDACTED
}
void ConfigManager::setAuthToken(std::string_view token) {
  if (!ensureStringsLoaded())
    return;
  Utils::copy_string(m_strings->AUTH_TOKEN, token);
}
void ConfigManager::setPortalPassword(std::string_view password) {
  if (!ensureStringsLoaded())
    return;
  Utils::copy_string(m_strings->PORTAL_PASSWORD, password);
}
void ConfigManager::setTimingConfig(const AppConfig& tempConfig) {
  m_config.DATA_UPLOAD_INTERVAL_MS = tempConfig.DATA_UPLOAD_INTERVAL_MS;
  m_config.SENSOR_SAMPLE_INTERVAL_MS = tempConfig.SENSOR_SAMPLE_INTERVAL_MS;
  m_config.CACHE_SEND_INTERVAL_MS = tempConfig.CACHE_SEND_INTERVAL_MS;
  m_config.SOFTWARE_WDT_TIMEOUT_MS = tempConfig.SOFTWARE_WDT_TIMEOUT_MS;
  validateAndSanitize();
}
void ConfigManager::setProvisioned(bool isProvisioned) {
  m_config.set_provisioned(isProvisioned);
}
void ConfigManager::setCalibration(float temp_offset, float humidity_offset, float lux_factor) {
  m_config.TEMP_OFFSET = temp_offset;
  m_config.HUMIDITY_OFFSET = humidity_offset;
  m_config.LUX_SCALING_FACTOR = lux_factor;
}
void ConfigManager::getHostname(char* buf, size_t len) const {
  if (!buf || len == 0)
    return;
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
  static constexpr char kHost[] = "gh-" STR(GH_ID) "-node-" STR(NODE_ID);
#undef STR
#undef STR_HELPER
  size_t n = sizeof(kHost) - 1;
  if (n > len - 1)
    n = len - 1;
  if (n > 0) {
    memcpy(buf, kHost, n);
  }
  buf[n] = '\0';
}
bool ConfigManager::factoryReset() {
  LOG_WARN("CONFIG", F("Formatting..."));
  return formatLittleFsSafe();
}
void ConfigManager::validateAndSanitize() {
  const uint32_t maxInterval = static_cast<uint32_t>(AppConstants::INTERVAL_MAX_MS);
  m_config.DATA_UPLOAD_INTERVAL_MS =
      min(max(m_config.DATA_UPLOAD_INTERVAL_MS, static_cast<uint32_t>(5000UL)), maxInterval);
  m_config.SENSOR_SAMPLE_INTERVAL_MS =
      min(max(m_config.SENSOR_SAMPLE_INTERVAL_MS, static_cast<uint32_t>(1000UL)), maxInterval);
  m_config.CACHE_SEND_INTERVAL_MS =
      min(max(m_config.CACHE_SEND_INTERVAL_MS, static_cast<uint32_t>(1000UL)), maxInterval);
  m_config.SOFTWARE_WDT_TIMEOUT_MS =
      min(max(m_config.SOFTWARE_WDT_TIMEOUT_MS, static_cast<uint32_t>(60000UL)), maxInterval);
}

bool ConfigManager::validateAndSanitizeStrings(AppConfigStrings& strings) {
  bool changed = false;
  if (!m_config.ALLOW_INSECURE_HTTPS()) {
    if (!isValidHttpsUrl(strings.DATA_UPLOAD_URL.data())) {
      Utils::copy_string(strings.DATA_UPLOAD_URL, DEFAULT_DATA_URL);
      changed = true;
    }
    if (!isValidHttpsUrl(strings.FW_VERSION_CHECK_URL_BASE.data())) {
      Utils::copy_string(strings.FW_VERSION_CHECK_URL_BASE, DEFAULT_OTA_URL_BASE);
      changed = true;
    }
  }

  auto sanitize = [&](std::span<char> buf) {
    for (char& c : buf) {
      if (c != '\0' && (c < 32 || c > 126)) {
        c = '?';
        changed = true;
      }
    }
  };

  sanitize(std::span{strings.AUTH_TOKEN});
  sanitize(std::span{strings.ADMIN_PASSWORD});
  sanitize(std::span{strings.PORTAL_PASSWORD});
  sanitize(std::span{strings.DATA_UPLOAD_URL});
  sanitize(std::span{strings.FW_VERSION_CHECK_URL_BASE});
  return changed;
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
  // FIX: Use strncpy_P for consistency with other path handling
  strncpy_P(path, WIFI_TEMP_PATH, sizeof(path));
  path[sizeof(path) - 1] = '\0';
  return LittleFS.exists(path);
}

// =============================================================================
// WiFi Credential Loading Helpers (Eliminate Duplication)
// =============================================================================

// Shared helper for loading credentials from a file
static void loadWifiCredentialsFrom(const char* path,
                                    std::span<char> ssid_buf,
                                    std::span<char> pass_buf,
                                    bool* hidden = nullptr) {
  if (!ssid_buf.empty())
    ssid_buf[0] = REDACTED
  if (!pass_buf.empty())
    pass_buf[0] = REDACTED
  if (hidden)
    *hidden = false;

  File f = LittleFS.open(path, "r");
  if (!f)
    return;

  WifiCredentials creds;
  if (f.read((uint8_t*)&creds, sizeof(creds)) == sizeof(creds)) {
    Utils::copy_string(ssid_buf, creds.ssid);
    if (hidden)
      *hidden = creds.hidden;

    // creds.pass is scrambled and may not be null-terminated. Copy raw bytes safely.
    if (!pass_buf.empty()) {
      memset(pass_buf.data(), 0, pass_buf.size());
      const size_t copy_len = std::min(pass_buf.size(), sizeof(creds.pass));
      memcpy(pass_buf.data(), creds.pass, copy_len);
      Utils::scramble_data(pass_buf.subspan(0, copy_len));
      pass_buf[pass_buf.size() - 1] = REDACTED
    }
  }
  f.close();
  Utils::trim_inplace(ssid_buf);
}

void ConfigManager::getWifiCredentials(std::span<char> ssid_buf, std::span<char> pass_buf) {
  char path[sizeof(WIFI_MAIN_PATH)];
  strncpy_P(path, WIFI_MAIN_PATH, sizeof(path));
  path[sizeof(path) - 1] = '\0';
  loadWifiCredentialsFrom(path, ssid_buf, pass_buf);
}

void ConfigManager::loadTempWifiCredentials(std::span<char> ssid_buf, std::span<char> pass_buf, bool& hidden) {
  char path[sizeof(WIFI_TEMP_PATH)];
  strncpy_P(path, WIFI_TEMP_PATH, sizeof(path));
  path[sizeof(path) - 1] = '\0';
  loadWifiCredentialsFrom(path, ssid_buf, pass_buf, &hidden);
}

bool ConfigManager::saveTempWifiCredentials(std::string_view ssid, std::string_view password, bool hidden) {
  WifiCredentials creds{};
  Utils::copy_string(std::span{creds.ssid}, ssid);
  Utils::copy_string(std::span{creds.pass}, password);
  creds.hidden = hidden;
  Utils::scramble_data(std::span{creds.pass});

  char path[sizeof(WIFI_TEMP_PATH)];
  strncpy_P(path, WIFI_TEMP_PATH, sizeof(path));
  path[sizeof(path) - 1] = '\0';

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
  strncpy_P(path, WIFI_TEMP_PATH, sizeof(path));
  path[sizeof(path) - 1] = '\0';
  LittleFS.remove(path);
}
