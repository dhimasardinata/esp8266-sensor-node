#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

#include <array>
#include <memory>
#include <span>
#include <string_view>

#include "IConfigObserver.h"

// Provisioning overrides (compile-time)
#ifndef DEFAULT_DATA_URL
#define DEFAULT_DATA_URL "https://example.com/api/sensor"
#endif
#ifndef DEFAULT_OTA_URL_BASE
#define DEFAULT_OTA_URL_BASE "REDACTED"
#endif
#ifndef DEFAULT_ADMIN_PASS_HASH
#define DEFAULT_ADMIN_PASS_HASH "REDACTED"
#endif
#ifndef DEFAULT_PORTAL_PASS
#define DEFAULT_PORTAL_PASS "REDACTED"
#endif
#ifndef DEFAULT_GATEWAY_IP
#define DEFAULT_GATEWAY_IP ""
#endif

// Configuration Constants
constexpr size_t MAX_PAYLOAD_SIZE = 256;
constexpr int MAX_CACHE_HEAD_RETRIES = 5;
constexpr unsigned long NTP_VALID_TIMESTAMP_THRESHOLD = 1704067200UL;
constexpr size_t MAX_CACHE_DATA_SIZE = 100 * 1024;

constexpr size_t MAX_TOKEN_LEN = REDACTED
constexpr size_t MAX_URL_LEN = 96;
constexpr size_t MAX_PASS_LEN = REDACTED
constexpr size_t MAX_WIFI_CRED_LEN = REDACTED

constexpr uint32_t CONFIG_MAGIC = 0xCF60B114;
constexpr uint16_t CONFIG_VERSION = 3;  // Bumped to 3 for CRC integrity

enum class ConfigStatus { OK, FILE_OPEN_FAILED, FILE_READ_ERROR, FILE_WRITE_FAILED, MAGIC_MISMATCH };

// ============================================================================
// AppConfig - Optimized for Memory Alignment
// ============================================================================
// Structure members are ordered from largest alignment to smallest to minimize
// padding bytes. ESP8266 requires 4-byte alignment for unsigned long and float.
//
// Before reordering: ~6 bytes wasted on padding
// After reordering: 0 bytes wasted (optimal packing)

struct ConfigFlags {
  uint8_t is_provisioned : 1;  // Was IS_PROVISIONED
  uint8_t allow_insecure : 1;  // Was ALLOW_INSECURE_HTTPS
  uint8_t log_level : 4;       // 0-15 range (was uint8_t)
  uint8_t _reserved : 2;       // Future use
};

struct AppConfig {
  // === 4-byte aligned members (largest first) ===
  uint32_t DATA_UPLOAD_INTERVAL_MS;
  uint32_t SENSOR_SAMPLE_INTERVAL_MS;
  uint32_t CACHE_SEND_INTERVAL_MS;
  uint32_t SOFTWARE_WDT_TIMEOUT_MS;

  float TEMP_OFFSET;
  float HUMIDITY_OFFSET;
  float LUX_SCALING_FACTOR;

  // === Packed flags (bit fields save 2+ bytes) ===
  ConfigFlags flags{};

  // Convenience accessors (maintain backward compatibility)
  bool IS_PROVISIONED() const { return flags.is_provisioned; }
  void set_provisioned(bool v) { flags.is_provisioned = v; }
  bool ALLOW_INSECURE_HTTPS() const { return flags.allow_insecure; }
  void set_insecure(bool v) { flags.allow_insecure = v; }
  uint8_t LOG_LEVEL() const { return flags.log_level; }
  void set_log_level(uint8_t v) { flags.log_level = v & 0x0F; }
};

struct AppConfigStrings {
  std::array<char, MAX_TOKEN_LEN> AUTH_TOKEN;
  std::array<char, MAX_URL_LEN> DATA_UPLOAD_URL;
  std::array<char, MAX_URL_LEN> FW_VERSION_CHECK_URL_BASE;
  std::array<char, MAX_PASS_LEN> ADMIN_PASSWORD;
  std::array<char, MAX_PASS_LEN> PORTAL_PASSWORD;
};

// ============================================================================
// Static Assert: Guard Against Padding Holes
// ============================================================================
// If this fails, someone reordered AppConfig fields incorrectly!
// Expected size breakdown:
//   4*unsigned long (16) + 3*float (12) + arrays (MAX_TOKEN+2*MAX_URL+2*MAX_PASS)
//   + 1 byte packed flags = 29 + 45 + 100*2 + 65*2 = 29 + 45 + 330 = 404 bytes
// Note: Actual size depends on MAX_* constants from limits.h
static_assert(sizeof(AppConfig) <= 64, "AppConfig has grown too large - check for padding holes!");

// ============================================================================
// Constexpr Factory Defaults (Zero-Cost Initialization)
// ============================================================================
#include "CompileTimeUtils.h"
#include "calibration.h"

namespace FactoryDefaults {
  using namespace CompileTimeUtils;
  
  // Default URLs and credentials (compile-time initialized)
  inline constexpr auto AUTH_TOKEN = REDACTED
  inline constexpr auto DATA_URL = ct_make_array<MAX_URL_LEN>(DEFAULT_DATA_URL);
  inline constexpr auto OTA_URL = REDACTED
  inline constexpr auto ADMIN_PASS = REDACTED
  inline constexpr auto PORTAL_PASS = REDACTED
  
  // Default intervals
  inline constexpr unsigned long UPLOAD_INTERVAL_MS = 600000UL;   // 10 minutes
  inline constexpr unsigned long SAMPLE_INTERVAL_MS = 60000UL;    // 1 minute
  inline constexpr unsigned long CACHE_INTERVAL_MS = 15000UL;
  inline constexpr unsigned long SW_WDT_TIMEOUT_MS = 1800000UL;   // 30 minutes
  
  // The runtime factory config (numeric + flags only)
  inline constexpr AppConfig CONFIG = {
    // 4-byte aligned members first
    UPLOAD_INTERVAL_MS,
    SAMPLE_INTERVAL_MS,
    CACHE_INTERVAL_MS,
    SW_WDT_TIMEOUT_MS,
    CompiledDefaults::TEMP_OFFSET,
    CompiledDefaults::HUMIDITY_OFFSET,
    CompiledDefaults::LUX_SCALING_FACTOR,
    // Packed flags: {is_provisioned, allow_insecure, log_level, _reserved}
    {1, 0, 1, 0}  // provisioned=true, insecure=false, log_level=INFO(1)
  };

  // Default strings (lazy-loaded)
  inline constexpr AppConfigStrings STRINGS = {
    AUTH_TOKEN,
    DATA_URL,
    OTA_URL,
    ADMIN_PASS,
    PORTAL_PASS,
  };
}

struct WifiCredentials {
  char ssid[MAX_WIFI_CRED_LEN];
  char pass[MAX_WIFI_CRED_LEN];
  bool hidden;
};

class ConfigManager {
public:
  ConfigManager();
  void init();
  void registerObserver(IConfigObserver* observer);

  // Load configuration from persistent storage.
  // Returns: ConfigStatus indicating success or type of failure.
  [[nodiscard]] ConfigStatus load();

  // Save current configuration to persistent storage.
  // Returns: ConfigStatus indicating success or type of failure.
  [[nodiscard]] ConfigStatus save();

  const AppConfig& getConfig() const;
  const AppConfigStrings& getStrings();
  void releaseStrings();
  [[nodiscard]] const char* getAuthToken();
  [[nodiscard]] const char* getDataUploadUrl();
  [[nodiscard]] const char* getOtaUrlBase();
  [[nodiscard]] const char* getAdminPassword();
  [[nodiscard]] const char* getPortalPassword();

  void setAuthToken(std:REDACTED
  void setPortalPassword(std:REDACTED
  void setTimingConfig(const AppConfig& tempConfig);
  void setProvisioned(bool isProvisioned);
  void setCalibration(float temp_offset, float humidity_offset, float lux_factor);

  void getHostname(char* buf, size_t len) const;
  // Reset all configuration to factory defaults.
  // Returns: true if reset was successful, false otherwise.
  [[nodiscard]] bool factoryReset();

  // Static Wifi Credential Helpers
  static void getWifiCredentials(std:REDACTED
  [[nodiscard]] static bool wifiCredentialsExist();
  [[nodiscard]] static bool tempWifiCredentialsExist();
  [[nodiscard]] static bool saveTempWifiCredentials(std:REDACTED
  static void loadTempWifiCredentials(std:REDACTED
  [[nodiscard]] static bool promoteTempWifiCredentials();
  static void clearTempWifiCredentials();

private:
  ConfigStatus loadFromFile(const char* path);
  void validateAndSanitize();
  [[nodiscard]] bool validateAndSanitizeStrings(AppConfigStrings& strings);
  [[nodiscard]] bool ensureStringsLoaded();
  void applyDefaults();
  void notifyObservers();

  AppConfig m_config;
  std::unique_ptr<AppConfigStrings> m_strings;
  std::array<IConfigObserver*, 4> m_observers{};
  uint8_t m_observerCount = 0;
};

#endif  // CONFIG_MANAGER_H
