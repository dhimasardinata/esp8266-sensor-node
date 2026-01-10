#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <IntervalTimer.h>

#include <vector>

#include "ConfigManager.h"
#include "WifiCredentialStore.h"

class IWifiStateObserver;

class WifiManager {
public:
  enum class State { 
    INITIALIZING, 
    SCANNING,           // Scanning for available networks
    CONNECTING_STA,     // Attempting connection
    CONNECTED_STA,      // Successfully connected
    TRYING_NEXT,        // Current credential failed, trying next
    PORTAL_MODE         // Captive portal active (with background retry)
  };

  WifiManager();

  // Deleted copy constructor
  WifiManager(const WifiManager&) = delete;
  WifiManager& operator=(const WifiManager&) = delete;

  void init(ConfigManager& configManager);
  void handle();
  [[nodiscard]] State getState() const noexcept;
  void startPortal();
  
  // Credential management (for portal UI)
  WifiCredentialStore& getCredentialStore() noexcept { return m_credentialStore; }
  [[nodiscard]] bool addUserCredential(std::string_view ssid, std::string_view password);
  [[nodiscard]] bool removeUserCredential(std::string_view ssid);
  
  // Force rescan and retry
  void triggerRescan();

  template <typename T>
  void registerObserver(T* observer) {
    if (observer) {
      if (m_observers.empty()) {
        m_observers.reserve(3);
      }
      m_observers.push_back(observer);
    }
  }

private:
  void startScan();
  void processScanResults();
  void tryNextCredential();
  void startConnectionAttempt(const WifiCredential* cred);
  void setState(State newState);

  ConfigManager* m_configManager = nullptr;
  WifiCredentialStore m_credentialStore;
  State m_wifiState = State::INITIALIZING;

  // Timers
  IntervalTimer m_connectTimeoutTimer;
  IntervalTimer m_backgroundRetryTimer;  // Retry while in portal
  IntervalTimer m_roamCheckTimer;
  IntervalTimer m_disconnectWdTimer;

  // Connection state
  const WifiCredential* m_currentCredential = nullptr;
  bool m_hasEverConnected = false;
  bool m_scanInProgress = false;
  
  // Roaming
  unsigned long m_lastRoamAttempt = 0;

  std::vector<IWifiStateObserver*> m_observers;
};

#endif
