#ifndef PORTAL_SERVER_H
#define PORTAL_SERVER_H

#include <DNSServer.h>
#include <system/IntervalTimer.h>

#include "system/ConfigManager.h"
#include "REDACTED"
#include "REDACTED"
static constexpr uint32_t SCAN_CACHE_DURATION_MS = 10000;

class AsyncWebServer;
class AsyncWebHandler;
class AsyncWebServerRequest;
class NtpClient;

class PortalServer : public IWifiStateObserver {
public:
  enum class PortalStatus { IDLE, TESTING, SUCCESS, FAIL, DECRYPTION_FAIL };

  PortalServer(AsyncWebServer& server, WifiManager& wifiManager, ConfigManager& configManager, NtpClient& ntpClient);

  PortalServer(const PortalServer&) = delete;
  PortalServer& operator=(const PortalServer&) = delete;

  void handle();
  void onWifiStateChanged(WifiManager:REDACTED

private:
  void begin();
  void stop();
  void setupRoutes();
  void handleSaveRequest(AsyncWebServerRequest* request);
  void handleScanRequest(AsyncWebServerRequest* request);
  void handleForgetRequest(AsyncWebServerRequest* request);
  void handleFactoryResetRequest(AsyncWebServerRequest* request);
  void handleTimeRequest(AsyncWebServerRequest* request);
  void sendStatusJson(AsyncWebServerRequest* request) const;
  void sendNetworksJson(AsyncWebServerRequest* request);
  void sendSavedCredentialsJson(AsyncWebServerRequest* request) const;
  void cacheNetworkScanResults();
  bool refreshCachedNetworksFromWifiManager();
  String templateProcessor(const String& var);

  // handle() helpers
  void handlePendingConnection();
  void startConnectionAttempt();
  void handleTestResult();


  AsyncWebServer& m_server;
  WifiManager& m_wifiManager;
  ConfigManager& m_configManager;
  NtpClient& m_ntpClient;
  DNSServer m_dnsServer;

  PortalStatus m_portalStatus = PortalStatus::IDLE;
  IntervalTimer m_portalTestTimer;
  IntervalTimer m_rebootTimer;
  bool m_reboot_scheduled = false;
  bool m_factoryResetPending = false;
  bool m_isRunning = false;
  bool m_pendingConnection = false;
  bool m_connectScheduled = false;
  uint32_t m_connectAt = 0;
  bool m_routesInitialized = false;

  uint32_t m_lastScanTime = 0;
  bool m_scanResultsCached = false;
  bool m_portalScanInProgress = false;
  uint32_t m_lastScanStart = 0;

  AsyncWebHandler* m_handlers[12];
  uint8_t m_handlerCount = 0;
};

#endif  // PORTAL_SERVER_H
