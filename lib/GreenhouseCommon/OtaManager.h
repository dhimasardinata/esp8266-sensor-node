// OtaManager.h
// Manages Over-The-Air (OTA) firmware updates from a remote server.
#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <IntervalTimer.h>
#include <memory>
#include <string_view>

class ConfigManager;
struct AppConfig;

// Forward declare dependencies
class NtpClient;
class WifiManager;
namespace BearSSL {
  class WiFiClientSecure;
  class X509List;
}

// OTA Constants.
constexpr long INITIAL_UPDATE_DELAY_MS = 2 * 60 * 1000;          // 2 minutes
constexpr long REGULAR_UPDATE_INTERVAL_MS = 1 * 60 * 60 * 1000;  // 1 hour

// Handles automated firmware update checks and application.
// Periodically checks the API server for new firmware versions.
// If a newer version is found, it downloads and applies the update securely via HTTPS.
class OtaManager {
public:
  // Constructor.
  // ntpClient: Reference to NtpClient ensuring time synchronization.
  // wifiManager: REDACTED
  // secureClient: Reference to shared WiFiClientSecure for HTTPS.
  // configManager: Reference to ConfigManager for configuration access.
  OtaManager(NtpClient& ntpClient,
             WifiManager& wifiManager,
             BearSSL::WiFiClientSecure& secureClient,
             ConfigManager& configManager,
             const BearSSL::X509List* trustAnchors);
  ~OtaManager();

  OtaManager(const OtaManager&) = REDACTED
  OtaManager& operator=REDACTED

  // Initializes internal timers for update checks.
  void init();

  // Applies configuration settings (e.g., interval).
  // config: AppConfig containing new settings.
  void applyConfig(const AppConfig& config);

  // Main processing loop.
  // Executes periodic update check logic.
  void handle();
  // Schedules a forced firmware update check on the next cycle.
  // Useful for triggering updates via diagnostic terminal.
  void forceUpdateCheck();

  // Forces an insecure OTA check (bypassing SSL verification).
  // CRITICAL: Use ONLY for recovery if certificates expire.
  void forceInsecureUpdate();
  void setTrustAnchors(const BearSSL::X509List* trustAnchors);
  void setUploadInProgress(bool inProgress);
  [[nodiscard]] bool isBusy() const;

private:
  [[nodiscard]] bool ensureTrustAnchors();
  [[nodiscard]] const BearSSL::X509List* activeTrustAnchors() const;
  [[nodiscard]] bool acquireTlsResources(bool allowInsecure);
  void releaseTlsResources();

  void checkForUpdates();

  // Helper to parse JSON manually without external library.
  void parseOtaJson(std:REDACTED
                    char* version,
                    size_t version_len,
                    char* url,
                    size_t url_len,
                    char* md5,
                    size_t md5_len,
                    int& status);

  NtpClient& m_ntpClient;
  WifiManager& m_wifiManager;
  BearSSL::WiFiClientSecure& m_secureClient;
  ConfigManager& m_configManager;
  const BearSSL::X509List* m_trustAnchors = nullptr;
  std::unique_ptr<BearSSL::X509List> m_localTrustAnchors;
  bool m_tlsActive = false;
  bool m_uploadInProgress = false;
  bool m_isBusy = false;

  // State Internal
  IntervalTimer m_updateCheckTimer;
  bool m_force_check = false;
  bool m_force_insecure = false;
  bool m_is_first_check = true;
};

#endif  // OTA_MANAGER_H
