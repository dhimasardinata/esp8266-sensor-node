#include "WifiManager.h"

#include <ESP8266WiFi.h>

#include "constants.h"
#include "IWifiStateObserver.h"
#include "Logger.h"

// Timer intervals
namespace {
    constexpr unsigned long CONNECT_TIMEOUT_MS = 15000;      // 15s per credential attempt
    constexpr unsigned long BACKGROUND_RETRY_MS = 30000;     // 30s between background scans in portal
    constexpr unsigned long ROAM_CHECK_INTERVAL_MS = 10000;  // 10s RSSI check
    constexpr unsigned long DISCONNECT_WD_MS = 30 * 60 * 1000; // 30min watchdog
    constexpr int32_t ROAM_THRESHOLD_DBM = -80;              // Trigger roam below this
    constexpr unsigned long ROAM_COOLDOWN_MS = 120000;       // 2min between roam attempts
}

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
        for (auto observer : m_observers) {
            observer->onWifiStateChanged(newState);
        }
    }
}

void WifiManager::startScan() {
    if (m_scanInProgress) return;
    
    LOG_INFO("WIFI", F("Scanning for networks..."));
    m_scanInProgress = true;
    setState(State::SCANNING);
    
    // Start async scan
    WiFi.scanNetworksAsync([](int result) {
        // Callback handled in handle() via scanComplete check
    }, true);  // true = show hidden networks
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
            // Should not stay here
            break;
            
        case State::SCANNING:
            processScanResults();
            break;
            
        case State::CONNECTING_STA:
        case State::TRYING_NEXT:
            if (WiFi.status() == WL_CONNECTED) {
                // SUCCESS!
                LOG_INFO("WIFI", F("CONNECTION SUCCESSFUL! SSID: %s, IP: %s, RSSI: %d dBm"),
                         WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
                
                // If we were in AP+STA mode, switch to STA only
                if (WiFi.getMode() == WIFI_AP_STA) {
                    LOG_DEBUG("WIFI", F("Closing Portal AP, switching to STA only."));
                    WiFi.mode(WIFI_STA);
                }
                
                setState(State::CONNECTED_STA);
                m_disconnectWdTimer.reset();
                m_hasEverConnected = true;
                
                // If connected via user-entered credentials, save them
                // (The portal will handle this via promoteTempWifiCredentials)
                
            } else if (m_connectTimeoutTimer.hasElapsed(false)) {
                // Connection timeout - try next credential
                LOG_WARN("WIFI", F("Timeout connecting to '%s'"), 
                        m_currentCredential ? m_currentCredential->ssid : "?");
                WiFi.disconnect(false);
                delay(100);
                tryNextCredential();
            }
            break;
            
        case State::CONNECTED_STA:
            // Check if still connected
            if (WiFi.status() != WL_CONNECTED) {
                LOG_WARN("WIFI", F("Connection lost! Rescanning..."));
                m_currentCredential = nullptr;
                startScan();
                return;
            }
            
            // Roaming check - look for better signal
            if (m_roamCheckTimer.hasElapsed()) {
                int32_t rssi = WiFi.RSSI();
                
                if (rssi < ROAM_THRESHOLD_DBM) {
                    if (millis() - m_lastRoamAttempt > ROAM_COOLDOWN_MS) {
                        LOG_INFO("WIFI", F("Weak signal (%d dBm). Scanning for better network..."), rssi);
                        m_lastRoamAttempt = millis();
                        
                        // Do a quick scan to see if better option exists
                        int n = WiFi.scanNetworks(false, true, 0, nullptr);
                        m_credentialStore.updateFromScan(n);
                        WiFi.scanDelete();
                        
                        // Check if primary/secondary has better signal
                        const auto* primary = m_credentialStore.getPrimaryGH();
                        const auto* secondary = m_credentialStore.getSecondaryGH();
                        
                        if (primary && primary->isAvailable && primary->lastRssi > rssi + 10) {
                            LOG_INFO("WIFI", F("Found better: '%s' (RSSI: %d). Switching..."), 
                                   primary->ssid, primary->lastRssi);
                            WiFi.disconnect(false);
                            startConnectionAttempt(primary);
                            return;
                        }
                        
                        if (secondary && secondary->isAvailable && secondary->lastRssi > rssi + 10) {
                            LOG_INFO("WIFI", F("Found better: '%s' (RSSI: %d). Switching..."), 
                                   secondary->ssid, secondary->lastRssi);
                            WiFi.disconnect(false);
                            startConnectionAttempt(secondary);
                            return;
                        }
                    }
                }
            }
            
            // Disconnect watchdog
            m_disconnectWdTimer.reset();  // Keep alive
            break;
            
        case State::PORTAL_MODE:
            // Portal is active, but we keep trying in background!
            
            // 1. Check if background connection succeeded
            if (WiFi.status() == WL_CONNECTED) {
                LOG_INFO("WIFI", F("Background connection successful!"));
                setState(State::CONNECTED_STA);
                WiFi.mode(WIFI_STA);  // Close portal AP
                LOG_INFO("WIFI", F("Portal closed. Device is now online."));
                return;
            }
            
            // 2. Periodic background scan and retry (autonomous operation)
            if (m_backgroundRetryTimer.hasElapsed()) {
                LOG_DEBUG("WIFI", F("Background: Rescanning for networks..."));
                
                // Quick sync scan
                int n = WiFi.scanNetworks(false, true, 0, nullptr);
                if (n > 0) {
                    m_credentialStore.updateFromScan(n);
                    WiFi.scanDelete();
                    
                    // Check if any known network is now available
                    size_t available = m_credentialStore.getTotalAvailableCount();
                    if (available > 0) {
                        LOG_INFO("WIFI", F("Background: Found %zu known networks. Attempting connection..."), available);
                        
                        m_credentialStore.resetConnectionAttempt();
                        const WifiCredential* cred = m_credentialStore.getNextCredential();
                        
                        if (cred) {
                            LOG_INFO("WIFI", F("Background: Trying '%s' (RSSI: %d)..."), cred->ssid, cred->lastRssi);
                            WiFi.begin(cred->ssid, cred->password);
                        }
                    }
                }
            }
            
            // 3. Watchdog - reboot if disconnected too long
            if (m_disconnectWdTimer.hasElapsed()) {
                LOG_ERROR("WIFI", F("Watchdog: Disconnected for 30 min. Rebooting..."));
                delay(1000);
                ESP.restart();
            }
            break;
    }
}

void WifiManager::startPortal() {
    // Already in portal mode
    if (m_wifiState == State::PORTAL_MODE) return;
    
    if (!m_configManager) {
        LOG_ERROR("WIFI", F("WifiManager::startPortal: ConfigManager is NULL!"));
        return;
    }
    
    LOG_INFO("WIFI", F("Opening Captive Portal (Background retry active)"));
    
    // Use AP+STA mode: AP for user config, STA for background connection
    WiFi.mode(WIFI_AP_STA);
    
    String apName = m_configManager->getHostname();
    const char* portalPassword = m_configManager->getConfig().PORTAL_PASSWORD.data();
    
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), 
                      IPAddress(192, 168, 4, 1), 
                      IPAddress(255, 255, 255, 0));
    WiFi.softAP(apName.c_str(), portalPassword);
    
    LOG_INFO("WIFI", F("AP Name: %s"), apName.c_str());
    LOG_INFO("WIFI", F("AP IP:   %s"), WiFi.softAPIP().toString().c_str());
    
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

bool WifiManager::addUserCredential(std::string_view ssid, std::string_view password) {
    return m_credentialStore.addCredential(ssid, password);
}

bool WifiManager::removeUserCredential(std::string_view ssid) {
    return m_credentialStore.removeCredential(ssid);
}