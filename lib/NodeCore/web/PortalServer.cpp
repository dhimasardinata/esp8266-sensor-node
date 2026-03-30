#include "web/PortalServer.h"

#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <cstring>

#include "REDACTED"
#include "system/Logger.h"
#include "net/NtpClient.h"
#include "REDACTED"

namespace {
  void scrub_buffer(char* buf, size_t len) {
    volatile char* p = reinterpret_cast<volatile char*>(buf);
    while (len--) {
      *p++ = 0;
    }
  }
}  // namespace

PortalServer::PortalServer(AsyncWebServer& server,
                           WifiManager& wifiManager,
                           ConfigManager& configManager,
                           NtpClient& ntpClient)
    : m_server(server),
      m_wifiManager(wifiManager),
      m_configManager(configManager),
      m_ntpClient(ntpClient),
      m_portalTestTimer(20000),
      m_rebootTimer(3000) {}

bool PortalServer::refreshCachedNetworksFromWifiManager() {
  if (!m_wifiManager.hasScanSnapshot()) {
    m_scanResultsCached = false;
    return false;
  }
  m_scanResultsCached = true;
  m_lastScanTime = millis();
  return true;
}

void PortalServer::onWifiStateChanged(WifiManager::State newState) {
  if (newState == WifiManager::State::PORTAL_MODE) {
    if (!m_isRunning)
      begin();
  } else {
    if (m_isRunning)
      stop();
  }
}

void PortalServer::begin() {
  if (m_isRunning)
    return;
  setupRoutes();
  m_portalStatus = PortalStatus::IDLE;
  m_reboot_scheduled = false;
  m_scanResultsCached = false;
  (void)refreshCachedNetworksFromWifiManager();
  m_dnsServer.start(53, "REDACTED", WiFi.softAPIP());
  m_isRunning = true;
  LOG_INFO("PORTAL", F("Portal Server started."));
}

void PortalServer::stop() {
  if (!m_isRunning)
    return;
  m_dnsServer.stop();
  m_isRunning = false;
  LOG_INFO("PORTAL", F("Portal Server stopped."));
}

// =============================================================================
// handle() Helper Methods
// =============================================================================

void PortalServer::handlePendingConnection() {
  m_pendingConnection = false;
  // Schedule a non-blocking connection attempt so mode changes can settle.
  m_connectScheduled = true;
  m_connectAt = millis() + 50;
}

void PortalServer::startConnectionAttempt() {
  m_connectScheduled = false;
  LOG_INFO("REDACTED", F("REDACTED"));

  // Keep AP alive during testing to avoid disconnecting the user.
  const uint8_t mode = WiFi.getMode();
  if ((mode & WIFI_AP) != REDACTED
    if ((mode & WIFI_STA) =REDACTED
      WiFi.mode(WIFI_AP_STA);
    }
  } else {
    WiFi.mode(WIFI_STA);
  }

  char s[64], p[64];
  bool h;
  ConfigManager::loadTempWifiCredentials(std::span{s}, std::span{p}, h);
  WiFi.begin(s, p);
  scrub_buffer(s, sizeof(s));
  scrub_buffer(p, sizeof(p));
}

void PortalServer::handleTestResult() {
  if (WiFi.status() =REDACTED
    m_portalStatus = PortalStatus::SUCCESS;

    // Save to permanent storage
    char s[64], p[64];
    bool h;
    ConfigManager::loadTempWifiCredentials(std::span{s}, std::span{p}, h);

    if (m_wifiManager.addUserCredential(s, p, h)) {
      LOG_INFO("PORTAL", F("Saved credentials for '%s' (hidden=%d)"), s, h);
    } else {
      LOG_ERROR("PORTAL", F("Failed to save credentials (full?)"));
    }

    ConfigManager::clearTempWifiCredentials();
    scrub_buffer(s, sizeof(s));
    scrub_buffer(p, sizeof(p));
    m_reboot_scheduled = true;
    m_rebootTimer.reset();
  } else if (m_portalTestTimer.hasElapsed(false)) {
    m_portalStatus = PortalStatus::FAIL;
    WiFi.disconnect(true);
    // FIX C: Ensure AP stays alive so user can retry
    LOG_WARN("PORTAL", F("Test connection failed. Restoring AP..."));
    constexpr uint32_t kPortalMinHeapForSta = 8000;
    constexpr uint32_t kPortalMinBlockForSta = 4000;
    const bool allowSta = (ESP.getFreeHeap() >= kPortalMinHeapForSta) &&
                          (ESP.getMaxFreeBlockSize() >= kPortalMinBlockForSta);
    const uint8_t mode = WiFi.getMode();
    if (allowSta) {
      if ((mode & WIFI_AP_STA) != REDACTED
        WiFi.mode(WIFI_AP_STA);
      }
    } else {
      if ((mode & WIFI_AP) =REDACTED
        WiFi.mode(WIFI_AP);
      } else if ((mode & WIFI_STA) != REDACTED
        WiFi.mode(WIFI_AP);
      }
    }
    ConfigManager::clearTempWifiCredentials();
  }
}

void PortalServer::handle() {
  if (!m_isRunning)
    return;
  m_dnsServer.processNextRequest();

  if (m_pendingConnection)
    handlePendingConnection();
  if (m_connectScheduled && (millis() - m_connectAt) < 0x80000000UL)
    startConnectionAttempt();
  if (m_portalStatus == PortalStatus::TESTING)
    handleTestResult();

  // Safety: Execute deferred Factory Reset in Main Loop (not ISR)
  if (m_factoryResetPending) {
    m_factoryResetPending = false;
    LOG_WARN("PORTAL", F("Factory Reset requested. Rebooting to Safe Mode..."));

    // Set the flag in RTC memory
    BootGuard::setRebootReason(BootGuard::RebootReason::FACTORY_RESET);

    // Give time for serial to flush
    delay(100);

    // Trigger Reboot
    ESP.restart();

    // Halt execution here
    while (true) {
      yield();
    }
  }

  if (m_reboot_scheduled && m_rebootTimer.hasElapsed()) {
    ESP.restart();
    m_rebootTimer.reset();
  }

  if (!m_portalScanInProgress &&
      m_scanResultsCached &&
      static_cast<int32_t>(millis() - m_lastScanTime) >= static_cast<int32_t>(SCAN_CACHE_DURATION_MS)) {
    m_scanResultsCached = false;
  }
}
