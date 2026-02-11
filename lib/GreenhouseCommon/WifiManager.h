#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <IntervalTimer.h>
#include <array>
#include <memory>
#include <user_interface.h>

class ConfigManager;
#include "REDACTED"

class IWifiStateObserver;

class WifiManager {
public:
  enum class State { 
    INITIALIZING, 
    SCANNING,           // Scanning for available networks.
    CONNECTING_STA,     // Attempting connection.
    CONNECTED_STA,      // Successfully connected.
    TRYING_NEXT,        // Connection failed, attempting next credential.
    PORTAL_MODE         // Captive portal active (user configuration required).
  };

  WifiManager();

  // Deleted copy constructor
  WifiManager(const WifiManager&) = REDACTED
  WifiManager& operator=REDACTED

  void init(ConfigManager& configManager);
  void handle();
  [[nodiscard]] State getState() const noexcept;
  [[nodiscard]] bool isScanBusy() const noexcept;
  void startPortal();
  void requestPortalScan();
  void releaseScanCache();

  struct WifiScanResult {
    int32_t rssi = 0;
    bool isOpen = false;
    char ssid[33] = REDACTED
  };
  static constexpr uint8_t MAX_SCAN_RESULTS = 4;
  uint8_t copyScanResults(WifiScanResult* out, uint8_t max) const;
  
  // Retrieve credential store for portal management.
  WifiCredentialStore& getCredentialStore() noexcept { return m_credentialStore; }
  [[nodiscard]] bool addUserCredential(std::string_view ssid, std::string_view password, bool hidden = false);
  [[nodiscard]] bool removeUserCredential(std::string_view ssid);
  
  // Trigger network rescan.
  void triggerRescan();

  template <typename T>
  void registerObserver(T* observer) {
    if (observer && m_observerCount < m_observers.size()) {
      m_observers[m_observerCount++] = observer;
    }
  }

private:
  void startScan();
  void processScanResults();
  void cacheScanResultsFromWifi(int scanCount);
  void tryNextCredential();
  void startConnectionAttempt(const WifiCredential* cred);
  void setState(State newState);
  void handleConnecting();
  void handleConnected();
  void handlePortalMode();
  void processRoamingScanResults(int scanCount);
  void configurePortalAp(bool allowSta);
  void startForcedPortalScan();
  void handleForcedPortalScanResult(int scanCount);
  void startLiteScanChannel(uint8_t channel);
  void handleLiteScanDone(void* arg, int status);
  void finalizeLiteScan();
  static void liteScanDoneThunk(void* arg, STATUS status);

  ConfigManager* m_configManager = nullptr;
  WifiCredentialStore m_credentialStore;
  State m_wifiState = REDACTED

  // Timers
  IntervalTimer m_connectTimeoutTimer;
  IntervalTimer m_backgroundRetryTimer;
  IntervalTimer m_roamCheckTimer;
  IntervalTimer m_disconnectWdTimer;

  // Connection state
  const WifiCredential* m_currentCredential = REDACTED
  WifiCredential m_activeCredential{};
  bool m_hasEverConnected = false;
  bool m_scanInProgress = false;
  bool m_initialScanPending = false;
  unsigned long m_initialScanAt = 0;
  std::unique_ptr<WifiScanResult[]> m_scanResults;
  uint8_t m_scanResultCount = 0;
  uint8_t m_scanResultCap = 0;
  uint8_t m_liteScanChannelIndex = 0;
  bool m_liteScanInProgress = false;
  unsigned long m_scanStartedAt = 0;
  bool m_forcePortalScan = false;
  bool m_forcePortalScanInProgress = false;
  unsigned long m_forcePortalScanAt = 0;
  unsigned long m_lastForcedPortalScan = 0;

  // Roaming
  unsigned long m_lastRoamAttempt = 0;
  bool m_roamingScanInProgress = false;
  int32_t m_roamingCurrentRssi = 0;

  std::array<IWifiStateObserver*, 8> m_observers;
  uint8_t m_observerCount = 0;

  bool ensureScanBuffer();
  void releaseScanBuffer();
};

#endif
