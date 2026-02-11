#include "REDACTED"

#include <ESP8266WiFi.h>
#include <user_interface.h>
#include <algorithm>
#include <cstring>

#include "ConfigManager.h"
#include "REDACTED"
#include "Logger.h"
#include "constants.h"
#include "utils.h"

// Timer intervals
namespace {
  constexpr unsigned long CONNECT_TIMEOUT_MS = 15000;         // 15s per credential attempt
  constexpr unsigned long BACKGROUND_RETRY_MS = 30000;        // 30s background scan interval in portal mode
  constexpr unsigned long ROAM_CHECK_INTERVAL_MS = 10000;     // 10s RSSI check interval
  constexpr unsigned long DISCONNECT_WD_MS = 30 * 60 * 1000;  // 30min disconnect watchdog
  constexpr int32_t ROAM_THRESHOLD_DBM = -80;                 // Roaming threshold (dBm)
  constexpr unsigned long ROAM_COOLDOWN_MS = 120000;          // 2min roaming cooldown
  constexpr unsigned long INITIAL_SCAN_DELAY_MS = 1500;
  constexpr uint32_t SCAN_MIN_HEAP = 7000;
  constexpr uint32_t SCAN_MIN_BLOCK = 3500;
  constexpr uint32_t SCAN_HIDDEN_MIN_HEAP = 10000;
  constexpr uint32_t PORTAL_BG_SCAN_MIN_HEAP = 9000;
  constexpr uint32_t PORTAL_BG_SCAN_MIN_BLOCK = 4500;
  constexpr unsigned long PORTAL_FORCED_SCAN_COOLDOWN_MS = 120000;
  constexpr unsigned long SCAN_TIMEOUT_MS = 15000;
  constexpr unsigned long LITE_SCAN_TIMEOUT_MS = 4000;
  constexpr uint8_t LITE_SCAN_CHANNELS[] = {1, 6, 11, 3, 9, 13};

  WifiManager* g_wifiManager = REDACTED
}  // namespace

WifiManager:REDACTED
    : m_connectTimeoutTimer(CONNECT_TIMEOUT_MS),
      m_backgroundRetryTimer(BACKGROUND_RETRY_MS),
      m_roamCheckTimer(ROAM_CHECK_INTERVAL_MS),
      m_disconnectWdTimer(DISCONNECT_WD_MS) {
  g_wifiManager = REDACTED
}

void WifiManager:REDACTED
  m_configManager = &configManager;
  if (!m_configManager) {
    LOG_ERROR("WIFI", F("WifiManager:REDACTED
    return;
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  // Initialize credential store
  m_credentialStore.init();

  LOG_INFO("REDACTED", F("REDACTED"));

  // Defer initial scan so other modules finish allocating.
  m_initialScanPending = true;
  m_initialScanAt = millis() + INITIAL_SCAN_DELAY_MS;
  setState(State::INITIALIZING);
}

WifiManager:REDACTED
  return m_wifiState;
}

bool WifiManager:REDACTED
  return m_scanInProgress || m_roamingScanInProgress || m_liteScanInProgress || m_forcePortalScanInProgress;
}

bool WifiManager:REDACTED
  if (m_scanResults) {
    return true;
  }
  std::unique_ptr<WifiScanResult[]> buf(new (std::nothrow) WifiScanResult[MAX_SCAN_RESULTS]());
  if (!buf) {
    LOG_WARN("REDACTED", F("REDACTED"));
    return false;
  }
  m_scanResults.swap(buf);
  m_scanResultCap = MAX_SCAN_RESULTS;
  return true;
}

void WifiManager:REDACTED
  m_scanResults.reset();
  m_scanResultCap = 0;
  m_scanResultCount = 0;
}

uint8_t WifiManager:REDACTED
  if (!out || max == 0 || m_scanResultCount == 0 || !m_scanResults)
    return 0;
  uint8_t count = m_scanResultCount < max ? m_scanResultCount : max;
  memcpy(out, m_scanResults.get(), sizeof(WifiScanResult) * count);
  return count;
}

void WifiManager:REDACTED
  m_forcePortalScan = true;
  m_forcePortalScanAt = millis() + 200;
}

void WifiManager:REDACTED
  if (isScanBusy()) {
    return;
  }
  releaseScanBuffer();
}

void WifiManager:REDACTED
  if (m_wifiState != REDACTED
    m_wifiState = REDACTED
    for (size_t i = 0; i < m_observerCount; ++i) {
      m_observers[i]->onWifiStateChanged(newState);
    }
  }
}

void WifiManager:REDACTED
  if (m_scanInProgress)
    return;

  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t freeBlock = ESP.getMaxFreeBlockSize();
  if (freeHeap < SCAN_MIN_HEAP || freeBlock < SCAN_MIN_BLOCK) {
    LOG_WARN("REDACTED",
             F("Scan skipped (low heap: %u, block %u). Opening Portal..."),
             freeHeap,
             freeBlock);
    startPortal();
    return;
  }

  const bool includeHidden = (freeHeap >= SCAN_HIDDEN_MIN_HEAP);
  LOG_INFO("REDACTED", F("REDACTED"));
  m_scanInProgress = true;
  m_scanStartedAt = millis();
  setState(State::SCANNING);

  // Initiate asynchronous network scan.
  WiFi.scanNetworksAsync(
      [](int result) {
        // Handled in handle() loop via scanComplete().
      },
      includeHidden);
}

void WifiManager:REDACTED
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_RUNNING) {
    if (m_scanStartedAt != 0 && (millis() - m_scanStartedAt) > SCAN_TIMEOUT_MS) {
      LOG_WARN("REDACTED", F("REDACTED"));
      WiFi.scanDelete();
      m_scanInProgress = false;
      m_scanStartedAt = 0;
      startScan();
    }
    return;  // Still scanning
  }

  m_scanInProgress = false;
  m_scanStartedAt = 0;

  if (n == WIFI_SCAN_FAILED || n < 0) {
    LOG_WARN("REDACTED", F("REDACTED"));
    yield();
    startScan();
    return;
  }

  LOG_INFO("REDACTED", F("REDACTED"), n);

  // Update credential store with scan results
  m_credentialStore.updateFromScan(n);
  m_credentialStore.resetConnectionAttempt();

  // Cache a small list of networks for portal display to avoid re-scans in low heap.
  cacheScanResultsFromWifi(n);

  // Clear scan results to free memory
  WiFi.scanDelete();

  // Try to connect to best available credential
  tryNextCredential();
}

void WifiManager:REDACTED
  m_scanResultCount = 0;
  if (scanCount <= 0) {
    return;
  }
  if (!ensureScanBuffer()) {
    return;
  }
  for (int i = 0; i < scanCount && m_scanResultCount < MAX_SCAN_RESULTS; i++) {
    String ssid = REDACTED
    if (ssid.length() =REDACTED
      continue;
    WifiScanResult& cached = REDACTED
    ssid.toCharArray(cached.ssid, sizeof(cached.ssid));
    cached.rssi = WiFi.RSSI(i);
    cached.isOpen = (WiFi.encryptionType(i) == ENC_TYPE_NONE);
  }
}

void WifiManager:REDACTED
  const WifiCredential* cred = REDACTED

  if (cred != nullptr) {
    setState(State::TRYING_NEXT);
    startConnectionAttempt(cred);
  } else {
    // No more credentials available
    LOG_WARN("REDACTED", F("REDACTED"));
    startPortal();
  }
}

void WifiManager:REDACTED
  if (!cred) {
    startPortal();
    return;
  }

  m_activeCredential = *cred;
  m_currentCredential = &m_activeCredential;

  LOG_INFO("REDACTED",
           F("Connecting to: '%s' (RSSI: %d dBm)"),
           m_activeCredential.ssid,
           m_activeCredential.lastRssi);

  WiFi.mode(WIFI_STA);
  WiFi.config(IPAddress(0, 0, 0, 0),
              IPAddress(0, 0, 0, 0),
              IPAddress(0, 0, 0, 0),
              IPAddress(8, 8, 8, 8),
              IPAddress(1, 1, 1, 1));
  WiFi.begin(m_activeCredential.ssid, m_activeCredential.password);

  setState(State::CONNECTING_STA);
  m_connectTimeoutTimer.reset();
}

void WifiManager:REDACTED
  switch (m_wifiState) {
    case State::INITIALIZING:
      if (m_initialScanPending &&
          static_cast<int32_t>(millis() - m_initialScanAt) >= 0) {
        m_initialScanPending = false;
        startScan();
      }
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

void WifiManager:REDACTED
  if (WiFi.status() =REDACTED
    char ssidBuf[WIFI_SSID_MAX_LEN] = REDACTED
    WiFi.SSID().toCharArray(ssidBuf, sizeof(ssidBuf));
    const char* ssid = REDACTED
    if ((ssid[0] =REDACTED
      ssid = REDACTED
    }

    int32_t rssi = WiFi.RSSI();
    IPAddress ip = WiFi.localIP();
    LOG_INFO("REDACTED",
             F("CONNECTED! SSID: REDACTED
             ssid,
             ip[0],
             ip[1],
             ip[2],
             ip[3],
             rssi);
    if (WiFi.getMode() =REDACTED
      WiFi.mode(WIFI_STA);
    setState(State::CONNECTED_STA);
    m_disconnectWdTimer.reset();
    m_hasEverConnected = true;
  } else if (m_connectTimeoutTimer.hasElapsed(false)) {
    const char* ssid = REDACTED
    LOG_WARN("REDACTED", F("REDACTED"), ssid);
    WiFi.disconnect(false);
    delay(100);
    tryNextCredential();
  }
}

void WifiManager:REDACTED
  if (WiFi.status() != REDACTED
    LOG_WARN("REDACTED", F("REDACTED"));
    m_currentCredential = nullptr;
    m_roamingScanInProgress = false;  // Reset roaming state
    startScan();
    return;
  }

  // Free scan cache while connected to keep heap for TLS/HTTP.
  if (!isScanBusy()) {
    releaseScanBuffer();
  }
  // Free saved credentials while connected; reload lazily if needed.
  m_credentialStore.releaseSavedCredentials();

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
      processRoamingScanResults(n);
    } else {
      LOG_WARN("REDACTED", F("REDACTED"));
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

  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t freeBlock = ESP.getMaxFreeBlockSize();
  if (freeHeap < SCAN_MIN_HEAP || freeBlock < SCAN_MIN_BLOCK) {
    LOG_WARN("REDACTED",
             F("Roam scan skipped (low heap: %u, block %u)"),
             freeHeap,
             freeBlock);
    m_disconnectWdTimer.reset();
    return;
  }

  // Start ASYNC roaming scan (non-blocking!)
  LOG_INFO("REDACTED", F("REDACTED"), rssi);
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

void WifiManager:REDACTED
  if (n <= 0)
    return;

  m_credentialStore.updateFromScan(n);

  const auto* primary = m_credentialStore.getPrimaryGH();
  const auto* secondary = m_credentialStore.getSecondaryGH();

  // Check if primary GH has significantly better signal
  if (primary && primary->isAvailable() && primary->lastRssi > m_roamingCurrentRssi + 10) {
    LOG_INFO("REDACTED",
             F("Roaming to '%s' (RSSI: %d vs current %d)"),
             primary->ssid,
             primary->lastRssi,
             m_roamingCurrentRssi);
    WiFi.disconnect(false);
    startConnectionAttempt(primary);
    return;
  }

  // Check secondary GH
  if (secondary && secondary->isAvailable() && secondary->lastRssi > m_roamingCurrentRssi + 10) {
    LOG_INFO("REDACTED",
             F("Roaming to '%s' (RSSI: %d vs current %d)"),
             secondary->ssid,
             secondary->lastRssi,
             m_roamingCurrentRssi);
    WiFi.disconnect(false);
    startConnectionAttempt(secondary);
    return;
  }

  // FIX: Also check user-saved networks for roaming
  for (const auto& cred : m_credentialStore.getSavedCredentials()) {
    if (!cred.isEmpty() && cred.isAvailable() && cred.lastRssi > m_roamingCurrentRssi + 10) {
      LOG_INFO("REDACTED",
               F("Roaming to saved '%s' (RSSI: %d vs current %d)"),
               cred.ssid,
               cred.lastRssi,
               m_roamingCurrentRssi);
      WiFi.disconnect(false);
      startConnectionAttempt(&cred);
      return;
    }
  }

  LOG_DEBUG("REDACTED", F("REDACTED"));
}

void WifiManager:REDACTED
  if (WiFi.status() =REDACTED
    LOG_INFO("REDACTED", F("REDACTED"));
    setState(State::CONNECTED_STA);
    WiFi.mode(WIFI_STA);
    m_scanInProgress = false;  // Ensure flag is cleared
    m_liteScanInProgress = false;
    m_forcePortalScanInProgress = false;
    m_scanStartedAt = 0;
    return;
  }

  // If STA is disabled, skip background scans to preserve heap for AP portal.
  if ((WiFi.getMode() & WIFI_STA) =REDACTED
    if (m_disconnectWdTimer.hasElapsed()) {
      LOG_ERROR("WIFI", F("Watchdog: REDACTED
      delay(1000);
      ESP.restart();
    }
    return;
  }

  if (m_liteScanInProgress) {
    if (m_scanStartedAt != 0 && (millis() - m_scanStartedAt) > LITE_SCAN_TIMEOUT_MS) {
      LOG_WARN("REDACTED", F("REDACTED"));
      finalizeLiteScan();
    }
    return;
  }

  // 1. Check if a background scan is currently running
  if (m_scanInProgress) {
    if (m_scanStartedAt != 0 && (millis() - m_scanStartedAt) > SCAN_TIMEOUT_MS) {
      LOG_WARN("REDACTED", F("REDACTED"));
      WiFi.scanDelete();
      m_scanInProgress = false;
      m_scanStartedAt = 0;
      if (m_forcePortalScanInProgress) {
        m_forcePortalScanInProgress = false;
        constexpr uint32_t kPortalMinHeapForSta = 8000;
        constexpr uint32_t kPortalMinBlockForSta = 4000;
        const bool allowSta = (ESP.getFreeHeap() >= kPortalMinHeapForSta) &&
                              (ESP.getMaxFreeBlockSize() >= kPortalMinBlockForSta);
        configurePortalAp(allowSta);
      }
      return;
    }
    int n = WiFi.scanComplete();
    if (n >= 0) {
      // Scan Finished
      LOG_INFO("WIFI", F("Background: REDACTED
      m_credentialStore.updateFromScan(n);
      WiFi.scanDelete();
      m_scanInProgress = false;
      m_scanStartedAt = 0;

      // Scan done, try to connect if we found something known
      if (m_credentialStore.getTotalAvailableCount() > 0) {
        m_credentialStore.resetConnectionAttempt();
        const WifiCredential* cred = REDACTED
        if (cred) {
          LOG_INFO("WIFI", F("Background: REDACTED
          WiFi.begin(cred->ssid, cred->password);
        }
      }
    } else if (n == WIFI_SCAN_FAILED) {
      LOG_WARN("WIFI", F("Background: REDACTED
      m_scanInProgress = false;
      m_scanStartedAt = 0;
      WiFi.scanDelete();
    }
    // If n == WIFI_SCAN_RUNNING (-1), just return and check next loop
  }
  // 2. Start new scan if timer elapsed and no scan running
  else if (m_backgroundRetryTimer.hasElapsed()) {
    if (!m_scanInProgress && !m_roamingScanInProgress) {  // Check BOTH flags
      // Avoid overlapping scans triggered by portal UI.
      if (WiFi.scanComplete() =REDACTED
        return;
      }
      if (WiFi.softAPgetStationNum() > 0) {
        LOG_DEBUG("REDACTED", F("REDACTED"));
        return;
      }
      const uint32_t freeHeap = ESP.getFreeHeap();
      const uint32_t freeBlock = ESP.getMaxFreeBlockSize();
      const bool portalLowHeap = (freeHeap < PORTAL_BG_SCAN_MIN_HEAP ||
                                  freeBlock < PORTAL_BG_SCAN_MIN_BLOCK);
      if (portalLowHeap) {
        const unsigned long now = millis();
        if ((now - m_lastForcedPortalScan) >= PORTAL_FORCED_SCAN_COOLDOWN_MS) {
          m_scanResultCount = 0;
          m_liteScanChannelIndex = 0;
          startLiteScanChannel(LITE_SCAN_CHANNELS[m_liteScanChannelIndex]);
        } else {
          LOG_DEBUG("REDACTED",
                    F("Background scan skipped (portal low heap: %u, block %u)"),
                    freeHeap,
                    freeBlock);
        }
        return;
      }
      if (freeHeap < SCAN_MIN_HEAP || freeBlock < SCAN_MIN_BLOCK) {
        LOG_WARN("REDACTED",
                 F("Background scan skipped (low heap: %u, block %u)"),
                 freeHeap,
                 freeBlock);
        return;
      }
      LOG_DEBUG("WIFI", F("Background: REDACTED
      WiFi.scanDelete();  // free any previous scan buffers
      WiFi.scanNetworksAsync(nullptr, false);  // skip hidden networks to reduce memory
      m_scanInProgress = true;
      m_scanStartedAt = millis();
    }
  }

  if (m_disconnectWdTimer.hasElapsed()) {
    LOG_ERROR("WIFI", F("Watchdog: REDACTED
    delay(1000);
    ESP.restart();
  }

  if (m_forcePortalScan &&
      !m_forcePortalScanInProgress &&
      !m_scanInProgress &&
      !m_roamingScanInProgress &&
      static_cast<int32_t>(millis() - m_forcePortalScanAt) >= 0) {
    m_scanResultCount = 0;
    m_liteScanChannelIndex = 0;
    startLiteScanChannel(LITE_SCAN_CHANNELS[m_liteScanChannelIndex]);
  }
}

void WifiManager:REDACTED
  // Already in portal mode
  if (m_wifiState =REDACTED
    return;

  if (!m_configManager) {
    LOG_ERROR("WIFI", F("WifiManager:REDACTED
    return;
  }

  LOG_INFO("REDACTED", F("REDACTED"));

  // Use AP+STA mode: AP for user configuration, STA for background connection attempts.
  WiFi.scanDelete();
  constexpr uint32_t kPortalMinHeapForSta = 8000;
  constexpr uint32_t kPortalMinBlockForSta = 4000;
  const bool allowSta = (ESP.getFreeHeap() >= kPortalMinHeapForSta) &&
                        (ESP.getMaxFreeBlockSize() >= kPortalMinBlockForSta);
  if (!allowSta) {
    LOG_WARN("REDACTED",
             F("Low heap: %u (block %u). Starting AP-only portal."),
             ESP.getFreeHeap(),
             ESP.getMaxFreeBlockSize());
  }
  configurePortalAp(allowSta);
  m_scanInProgress = false;
  m_roamingScanInProgress = false;
  m_liteScanInProgress = false;
  m_forcePortalScanInProgress = false;
  m_scanStartedAt = 0;

  // Reset background timers.
  m_backgroundRetryTimer.reset();
  m_disconnectWdTimer.reset();

  setState(State::PORTAL_MODE);
}

void WifiManager:REDACTED
  LOG_INFO("REDACTED", F("REDACTED"));
  m_currentCredential = nullptr;
  startScan();
}

bool WifiManager:REDACTED
  return m_credentialStore.addCredential(ssid, password, hidden);
}

bool WifiManager:REDACTED
  return m_credentialStore.removeCredential(ssid);
}

void WifiManager:REDACTED
  WiFi.mode(allowSta ? WIFI_AP_STA : REDACTED

  char apName[32];
  m_configManager->getHostname(apName, sizeof(apName));
  const char* portalPassword = REDACTED
  size_t portalPassLen = REDACTED
  const bool passValid = REDACTED
  const char* apPass = REDACTED
  if (!passValid) {
    LOG_WARN("REDACTED", F("REDACTED"));
  }
  if (!allowSta && apPass) {
    LOG_WARN("REDACTED", F("REDACTED"));
    apPass = REDACTED
  }

  WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                    IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  bool apOk = WiFi.softAP(apName, apPass, 1, false, 1);
  if (!apOk) {
    LOG_ERROR("REDACTED", F("REDACTED"));
    (void)WiFi.softAP(apName, nullptr, 1, false, 1);
  }

  m_configManager->releaseStrings();

  LOG_INFO("WIFI", F("AP Name: REDACTED
  IPAddress ip = WiFi.softAPIP();
  LOG_INFO("WIFI", F("AP IP:   REDACTED
}

void WifiManager:REDACTED
  m_forcePortalScan = false;
  m_forcePortalScanInProgress = true;
  m_liteScanInProgress = false;
  m_lastForcedPortalScan = millis();
  LOG_WARN("WIFI", F("Portal scan: REDACTED
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  delay(50);
  WiFi.scanDelete();
  WiFi.scanNetworksAsync(
      [this](int result) { this->handleForcedPortalScanResult(result); },
      false);
  m_scanInProgress = true;
  m_scanStartedAt = millis();
}

void WifiManager:REDACTED
  m_scanInProgress = false;
  m_forcePortalScanInProgress = false;
  m_scanStartedAt = 0;

  if (scanCount >= 0) {
    m_credentialStore.updateFromScan(scanCount);
    cacheScanResultsFromWifi(scanCount);
  }
  WiFi.scanDelete();

  constexpr uint32_t kPortalMinHeapForSta = 8000;
  constexpr uint32_t kPortalMinBlockForSta = 4000;
  const bool allowSta = (ESP.getFreeHeap() >= kPortalMinHeapForSta) &&
                        (ESP.getMaxFreeBlockSize() >= kPortalMinBlockForSta);
  configurePortalAp(allowSta);
}

void WifiManager:REDACTED
  if (m_scanInProgress)
    return;
  if (m_forcePortalScanInProgress && !m_liteScanInProgress)
    return;
  if (!ensureScanBuffer()) {
    LOG_WARN("REDACTED", F("REDACTED"));
    return;
  }

  m_forcePortalScan = false;
  m_forcePortalScanInProgress = true;
  m_liteScanInProgress = true;
  m_lastForcedPortalScan = millis();
  m_scanStartedAt = millis();

  LOG_WARN("WIFI", F("Lite scan ch%u: REDACTED
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  delay(50);

  scan_config config{};
  config.ssid = REDACTED
  config.bssid = REDACTED
  config.channel = channel;
  config.show_hidden = 0;
  if (!wifi_station_scan(&config, WifiManager:REDACTED
    LOG_WARN("REDACTED", F("REDACTED"));
    finalizeLiteScan();
    return;
  }
  m_scanInProgress = true;
}

void WifiManager:REDACTED
  m_scanInProgress = false;
  if (!m_scanResults && !ensureScanBuffer()) {
    LOG_WARN("REDACTED", F("REDACTED"));
    finalizeLiteScan();
    return;
  }

  if (status == OK && arg) {
    bss_info* bss = static_cast<bss_info*>(arg);
    while (bss) {
      if (bss->ssid_len > 0 && m_scanResultCount < MAX_SCAN_RESULTS) {
        const size_t ssidLen = REDACTED
        bool exists = false;
        for (uint8_t i = 0; i < m_scanResultCount; ++i) {
          if (strncmp(m_scanResults[i].ssid, reinterpret_cast<const char*>(bss->ssid), ssidLen) =REDACTED
              m_scanResults[i].ssid[ssidLen] =REDACTED
            exists = true;
            if (bss->rssi > m_scanResults[i].rssi) {
              m_scanResults[i].rssi = bss->rssi;
              m_scanResults[i].isOpen = (bss->authmode == AUTH_OPEN);
            }
            break;
          }
        }
        if (!exists && m_scanResultCount < MAX_SCAN_RESULTS) {
          WifiScanResult& dst = REDACTED
          memcpy(dst.ssid, bss->ssid, ssidLen);
          dst.ssid[ssidLen] = REDACTED
          dst.rssi = bss->rssi;
          dst.isOpen = (bss->authmode == AUTH_OPEN);
        }
      }
      bss = STAILQ_NEXT(bss, next);
    }
  }

  m_liteScanChannelIndex++;
  if (m_liteScanChannelIndex < (sizeof(LITE_SCAN_CHANNELS) / sizeof(LITE_SCAN_CHANNELS[0])) &&
      m_scanResultCount < MAX_SCAN_RESULTS) {
    startLiteScanChannel(LITE_SCAN_CHANNELS[m_liteScanChannelIndex]);
    return;
  }

  finalizeLiteScan();
}

void WifiManager:REDACTED
  m_liteScanInProgress = false;
  m_forcePortalScanInProgress = false;
  m_scanStartedAt = 0;
  m_scanInProgress = false;
  m_liteScanChannelIndex = 0;

  if (m_scanResultCount > 0 && m_scanResults) {
    std::array<WifiCredentialStore::ScanEntry, MAX_SCAN_RESULTS> entries{};
    for (uint8_t i = 0; i < m_scanResultCount; ++i) {
      entries[i].ssid = REDACTED
      entries[i].len = static_cast<uint8_t>(strnlen(m_scanResults[i].ssid, WIFI_SSID_MAX_LEN - 1));
      entries[i].rssi = m_scanResults[i].rssi;
    }
    m_credentialStore.updateFromScanList(entries.data(), m_scanResultCount);
  }

  constexpr uint32_t kPortalMinHeapForSta = 8000;
  constexpr uint32_t kPortalMinBlockForSta = 4000;
  const bool allowSta = (ESP.getFreeHeap() >= kPortalMinHeapForSta) &&
                        (ESP.getMaxFreeBlockSize() >= kPortalMinBlockForSta);
  configurePortalAp(allowSta);
}

void WifiManager:REDACTED
  if (g_wifiManager) {
    g_wifiManager->handleLiteScanDone(arg, static_cast<int>(status));
  }
}
