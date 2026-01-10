#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

#include <array>
#include <span>
#include <string_view>
#include <vector>

#include "IConfigObserver.h"

// Configuration Constants
constexpr size_t MAX_PAYLOAD_SIZE = 384;
constexpr int MAX_CACHE_HEAD_RETRIES = 5;
constexpr unsigned long NTP_VALID_TIMESTAMP_THRESHOLD = 1704067200UL;
constexpr size_t MAX_CACHE_DATA_SIZE = 100 * 1024;

constexpr size_t MAX_TOKEN_LEN = 45;
constexpr size_t MAX_URL_LEN = 96;
constexpr size_t MAX_PASS_LEN = 65;
constexpr size_t MAX_WIFI_CRED_LEN = 64;

constexpr uint32_t CONFIG_MAGIC = 0xCF60B114;
constexpr uint16_t CONFIG_VERSION = 2;  // Bumped to 2 for Insecure Flag

enum class ConfigStatus { OK, FILE_OPEN_FAILED, FILE_READ_ERROR, FILE_WRITE_FAILED, MAGIC_MISMATCH };

struct AppConfig {
  std::array<char, MAX_TOKEN_LEN> AUTH_TOKEN;
  std::array<char, MAX_URL_LEN> DATA_UPLOAD_URL;
  std::array<char, MAX_URL_LEN> FW_VERSION_CHECK_URL_BASE;
  std::array<char, MAX_PASS_LEN> ADMIN_PASSWORD;
  std::array<char, MAX_PASS_LEN> PORTAL_PASSWORD;
  bool IS_PROVISIONED;
  bool ALLOW_INSECURE_HTTPS;  // NEW in V2

  unsigned long DATA_UPLOAD_INTERVAL_MS;
  unsigned long SENSOR_SAMPLE_INTERVAL_MS;
  unsigned long CACHE_SEND_INTERVAL_MS;
  unsigned long SOFTWARE_WDT_TIMEOUT_MS;

  float TEMP_OFFSET;
  float HUMIDITY_OFFSET;
  float LUX_SCALING_FACTOR;

  uint8_t LOG_LEVEL;  ///< Runtime log level (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 255=NONE)
};

struct WifiCredentials {
  char ssid[MAX_WIFI_CRED_LEN];
  char pass[MAX_WIFI_CRED_LEN];
};

class ConfigManager {
public:
  ConfigManager();
  void init();
  void registerObserver(IConfigObserver* observer);

  /**
   * @brief Load configuration from persistent storage.
   * @return ConfigStatus indicating success or type of failure.
   */
  [[nodiscard]] ConfigStatus load();

  /**
   * @brief Save current configuration to persistent storage.
   * @return ConfigStatus indicating success or type of failure.
   */
  [[nodiscard]] ConfigStatus save();

  const AppConfig& getConfig() const;

  void setAuthToken(std::string_view token);
  void setPortalPassword(std::string_view password);
  void setTimingConfig(const AppConfig& tempConfig);
  void setProvisioned(bool isProvisioned);
  void setCalibration(float temp_offset, float humidity_offset, float lux_factor);

  String getHostname() const;
  /**
   * @brief Reset all configuration to factory defaults.
   * @return true if reset was successful, false otherwise.
   */
  [[nodiscard]] bool factoryReset();

  // Static Wifi Credential Helpers
  static void getWifiCredentials(std::span<char> ssid_buf, std::span<char> pass_buf);
  [[nodiscard]] static bool wifiCredentialsExist();
  [[nodiscard]] static bool tempWifiCredentialsExist();
  [[nodiscard]] static bool saveTempWifiCredentials(std::string_view ssid, std::string_view password);
  static void loadTempWifiCredentials(std::span<char> ssid_buf, std::span<char> pass_buf);
  [[nodiscard]] static bool promoteTempWifiCredentials();
  static void clearTempWifiCredentials();

private:
  ConfigStatus loadFromFile(const char* path);
  void validateAndSanitize();
  void applyDefaults();
  void notifyObservers();

  AppConfig m_config;
  std::vector<IConfigObserver*> m_observers;
};

#endif  // CONFIG_MANAGER_H