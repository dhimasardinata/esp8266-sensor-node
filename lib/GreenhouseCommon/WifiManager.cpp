#include "WifiManager.h"

#include <ESP8266WiFi.h>

#include "ConfigManager.h"
#include "IWifiStateObserver.h"
#include "Logger.h"
#include "constants.h"

// Timer intervals
namespace {
  constexpr unsigned long CONNECT_TIMEOUT_MS = 15000;         // 15s per credential attempt
  constexpr unsigned long BACKGROUND_RETRY_MS = 30000;        // 30s between background scans in portal
  constexpr unsigned long ROAM_CHECK_INTERVAL_MS = 10000;     // 10s RSSI check
  constexpr unsigned long DISCONNECT_WD_MS = 30 * 60 * 1000;  // 30min watchdog
  constexpr int32_t ROAM_THRESHOLD_DBM = -80;                 // Trigger roam below this
  constexpr unsigned long ROAM_COOLDOWN_MS = 120000;          // 2min between roam attempts
}  // namespace

WifiManager::WifiManager()
    : m_connectTimeoutTimer(CONNECT_TIMEOUT_MS),
      m_backgroundRetryTimer(BACKGROUND_RETRY_MS),
      m_roamCheckTimer(ROAM_CHECK_INTERVAL_MS),
      m_disconnectWdTimer(DISCONNECT_WD_MS) {}

void WifiManager::init(ConfigManager& configManager) {
  m_configManager = &configManager;
  if (!m_configManager) {
    LOG_ERROR("WIFI", F("WifiManager::init: ConfigManager reference is invalid."));
    return;
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  // Initialize credential store
  m_credentialStore.init();

  LOG_INFO("WIFI", F("Smart WiFi Manager Initialized"));

  // Start by scanning for networks
  startScan();
}

WifiManager::State WifiManager::getState() const noexcept {
  return m_wifiState;
}

void WifiManager::setState(State newState) {
  if (m_wifiState != newState) {
    m_wifiState = newState;
    for (size_t i = 0; i < m_observerCount; ++i) {
      m_observers[i]->onWifiStateChanged(newState);
    }
  }
}

void WifiManager::startScan() {
  if (m_scanInProgress)
    return;

  LOG_INFO("WIFI", F("Scanning for networks..."));
  m_scanInProgress = true;
  setState(State::SCANNING);

  // Start async scan
  WiFi.scanNetworksAsync(
      [](int result) {
        // Callback handled in handle() via scanComplete check
      },
      true);  // true = show hidden networks
}

void WifiManager::processScanResults() {
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_RUNNING) {
    return;  // Still scanning
  }

  m_scanInProgress = false;

  if (n == WIFI_SCAN_FAILED || n < 0) {
    LOG_WARN("WIFI", F("Scan failed. Retrying..."));
    delay(100);
    startScan();
    return;
  }

  LOG_INFO("WIFI", F("Found %d networks"), n);

  // Update credential store with scan results
  m_credentialStore.updateFromScan(n);
  m_credentialStore.resetConnectionAttempt();

  // Clear scan results to free memory
  WiFi.scanDelete();

  // Try to connect to best available credential
  tryNextCredential();
}

void WifiManager::tryNextCredential() {
  const WifiCredential* cred = m_credentialStore.getNextCredential();

  if (cred != nullptr) {
    setState(State::TRYING_NEXT);
    startConnectionAttempt(cred);
  } else {
    // No more credentials available
    LOG_WARN("WIFI", F("No available networks. Opening Portal..."));
    startPortal();
  }
}

void WifiManager::startConnectionAttempt(const WifiCredential* cred) {
  if (!cred) {
    startPortal();
    return;
  }

  m_currentCredential = cred;

  LOG_INFO("WIFI", F("Connecting to: '%s' (RSSI: %d dBm)"), cred->ssid, cred->lastRssi);

  WiFi.mode(WIFI_STA);
  WiFi.config(IPAddress(0, 0, 0, 0),
              IPAddress(0, 0, 0, 0),
              IPAddress(0, 0, 0, 0),
              IPAddress(8, 8, 8, 8),
              IPAddress(1, 1, 1, 1));
  WiFi.begin(cred->ssid, cred->password);

  setState(State::CONNECTING_STA);
  m_connectTimeoutTimer.reset();
}

void WifiManager::handle() {
  switch (m_wifiState) {
    case State::INITIALIZING:
      break;
    case State::SCANNING:
      processScanResults();
      break;
    case State::CONNECTING_STA:
    case State::TRYING_NEXT:
      handleConnecting();
      break;
    case State::CONNECTED_STA:
      handleConnected();
      break;
    case State::PORTAL_MODE:
      handlePortalMode();
      break;
  }
}

void WifiManager::handleConnecting() {
  if (WiFi.status() == WL_CONNECTED) {
    const char* ssid = WiFi.SSID().c_str(); 
    if ((!ssid || ssid[0] == '\0') && m_currentCredential && !m_currentCredential->isEmpty()) { 
       ssid = m_currentCredential->ssid; 
    }
    
    IPAddress ip = WiFi.localIP();
    int32_t rssi = WiFi.RSSI();
    LOG_INFO("WIFI", F("CONNECTED! SSID: %s, IP: %d.%d.%d.%d, RSSI: %d dBm"), 
             ssid, ip[0], ip[1], ip[2], ip[3], rssi);
    if (WiFi.getMode() == WIFI_AP_STA)
      WiFi.mode(WIFI_STA);
    setState(State::CONNECTED_STA);
    m_disconnectWdTimer.reset();
    m_hasEverConnected = true;
  } else if (m_connectTimeoutTimer.hasElapsed(false)) {
    LOG_WARN("WIFI", F("Timeout connecting to '%s'"), m_currentCredential ? m_currentCredential->ssid : "?");
    WiFi.disconnect(false);
    delay(100);
    tryNextCredential();
  }
}

void WifiManager::handleConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    LOG_WARN("WIFI", F("Connection lost! Rescanning..."));
    m_currentCredential = nullptr;
    m_roamingScanInProgress = false;  // Reset roaming state
    startScan();
    return;
  }

  // If a roaming scan is in progress, check for results
  if (m_roamingScanInProgress) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
      // Still scanning, don't block - just return
      m_disconnectWdTimer.reset();
      return;
    }
    // Scan finished (success or fail), process results
    m_roamingScanInProgress = false;
    if (n >= 0) {
      processRoamingScanResults();
    } else {
      LOG_WARN("WIFI", F("Roaming scan failed"));
    }
    WiFi.scanDelete();
    m_disconnectWdTimer.reset();
    return;
  }

  if (!m_roamCheckTimer.hasElapsed()) {
    m_disconnectWdTimer.reset();
    return;
  }

  int32_t rssi = WiFi.RSSI();
  if (rssi >= ROAM_THRESHOLD_DBM || millis() - m_lastRoamAttempt < ROAM_COOLDOWN_MS) {
    m_disconnectWdTimer.reset();
    return;
  }

  // Start ASYNC roaming scan (non-blocking!)
  LOG_INFO("WIFI", F("Weak signal (%d dBm). Starting async scan..."), rssi);
  m_lastRoamAttempt = millis();
  m_roamingCurrentRssi = rssi;  // Save current RSSI for comparison later
  m_roamingScanInProgress = true;
  
  WiFi.scanNetworksAsync(
      [](int /*result*/) {
        // Callback is just a trigger - actual processing in next handle() loop
      },
      true);  // true = show hidden networks
  
  m_disconnectWdTimer.reset();
}

void WifiManager::processRoamingScanResults() {
  int n = WiFi.scanComplete();
  if (n <= 0) return;
  
  m_credentialStore.updateFromScan(n);

  const auto* primary = m_credentialStore.getPrimaryGH();
  const auto* secondary = m_credentialStore.getSecondaryGH();

  // Check if primary GH has significantly better signal
  if (primary && primary->isAvailable && primary->lastRssi > m_roamingCurrentRssi + 10) {
    LOG_INFO("WIFI", F("Roaming to '%s' (RSSI: %d vs current %d)"), 
             primary->ssid, primary->lastRssi, m_roamingCurrentRssi);
    WiFi.disconnect(false);
    startConnectionAttempt(primary);
    return;
  }
  
  // Check secondary GH
  if (secondary && secondary->isAvailable && secondary->lastRssi > m_roamingCurrentRssi + 10) {
    LOG_INFO("WIFI", F("Roaming to '%s' (RSSI: %d vs current %d)"), 
             secondary->ssid, secondary->lastRssi, m_roamingCurrentRssi);
    WiFi.disconnect(false);
    startConnectionAttempt(secondary);
    return;
  }
  
  LOG_DEBUG("WIFI", F("No better network found. Staying connected."));
}

void WifiManager::handlePortalMode() {
  if (WiFi.status() == WL_CONNECTED) {
    LOG_INFO("WIFI", F("Background connection successful!"));
    setState(State::CONNECTED_STA);
    WiFi.mode(WIFI_STA);
    return;
  }

  if (m_backgroundRetryTimer.hasElapsed()) {
    LOG_DEBUG("WIFI", F("Background: Rescanning..."));
    int n = WiFi.scanNetworks(false, true, 0, nullptr);
    if (n > 0) {
      m_credentialStore.updateFromScan(n);
      WiFi.scanDelete();
      if (m_credentialStore.getTotalAvailableCount() > 0) {
        m_credentialStore.resetConnectionAttempt();
        const WifiCredential* cred = m_credentialStore.getNextCredential();
        if (cred) {
          LOG_INFO("WIFI", F("Background: Trying '%s' (RSSI: %d)..."), cred->ssid, cred->lastRssi);
          WiFi.begin(cred->ssid, cred->password);
        }
      }
    }
  }

  if (m_disconnectWdTimer.hasElapsed()) {
    LOG_ERROR("WIFI", F("Watchdog: Disconnected for 30 min. Rebooting..."));
    delay(1000);
    ESP.restart();
  }
}

void WifiManager::startPortal() {
  // Already in portal mode
  if (m_wifiState == State::PORTAL_MODE)
    return;

  if (!m_configManager) {
    LOG_ERROR("WIFI", F("WifiManager::startPortal: ConfigManager is NULL!"));
    return;
  }

  LOG_INFO("WIFI", F("Opening Captive Portal (Background retry active)"));

  // Use AP+STA mode: AP for user config, STA for background connection
  WiFi.mode(WIFI_AP_STA);

  char apName[32];
  m_configManager->getHostname(apName, sizeof(apName));
  const char* portalPassword = m_configManager->getConfig().PORTAL_PASSWORD.data();

  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName, portalPassword);

  LOG_INFO("WIFI", F("AP Name: %s"), apName);
  IPAddress ip = WiFi.softAPIP();
  LOG_INFO("WIFI", F("AP IP:   %u.%u.%u.%u"), ip[0], ip[1], ip[2], ip[3]);

  // Start background retry timer
  m_backgroundRetryTimer.reset();
  m_disconnectWdTimer.reset();

  setState(State::PORTAL_MODE);
}

void WifiManager::triggerRescan() {
  LOG_INFO("WIFI", F("Manual rescan triggered."));
  m_currentCredential = nullptr;
  startScan();
}

bool WifiManager::addUserCredential(std::string_view ssid, std::string_view password, bool hidden) {
  return m_credentialStore.addCredential(ssid, password, hidden);
}

bool WifiManager::removeUserCredential(std::string_view ssid) {
  return m_credentialStore.removeCredential(ssid);
}