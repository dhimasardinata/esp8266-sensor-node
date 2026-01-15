#ifndef PORTAL_SERVER_H
#define PORTAL_SERVER_H

#include <DNSServer.h>
#include <IntervalTimer.h>
#include "ConfigManager.h"
#include "IWifiStateObserver.h"
#include "WifiManager.h"

class AsyncWebServer;
class AsyncWebHandler;
class AsyncWebServerRequest;

class PortalServer : public IWifiStateObserver {
public:
  enum class PortalStatus { IDLE, TESTING, SUCCESS, FAIL };

  PortalServer(AsyncWebServer& server, WifiManager& wifiManager, ConfigManager& configManager);
  
  PortalServer(const PortalServer&) = delete;
  PortalServer& operator=(const PortalServer&) = delete;

  void handle();
  void onWifiStateChanged(WifiManager::State newState) override;

private:
  void begin();
  void stop();
  void setupRoutes();
  void sendStatusJson(AsyncWebServerRequest* request) const;
  void sendNetworksJson(AsyncWebServerRequest* request) const;
  String getSavedCredentialsJson() const;
  String templateProcessor(const String& var);
  
  // handle() helpers
  void handlePendingConnection();
  void handleTestResult();
  
  // sendNetworksJson helper
  static int computeSignalBars(int32_t rssi);

  AsyncWebServer& m_server;
  WifiManager& m_wifiManager;
  ConfigManager& m_configManager;
  DNSServer m_dnsServer;

  PortalStatus m_portalStatus = PortalStatus::IDLE;
  IntervalTimer m_portalTestTimer;
  IntervalTimer m_rebootTimer;
  bool m_reboot_scheduled = false;
  bool m_isRunning = false;
  bool m_pendingConnection = false;
  
  AsyncWebHandler* m_handlers[12];
  uint8_t m_handlerCount = 0;
};

#endif // PORTAL_SERVER_H