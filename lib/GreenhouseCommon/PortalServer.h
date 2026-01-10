#ifndef PORTAL_SERVER_H
#define PORTAL_SERVER_H

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <IntervalTimer.h>

#include "ConfigManager.h"
#include "IWifiStateObserver.h"  // <-- Include the observer interface
#include "WifiManager.h"

class PortalServer : public IWifiStateObserver {  // <-- Inherit from the interface
public:
  enum class PortalStatus { IDLE, TESTING, SUCCESS, FAIL };

  PortalServer(AsyncWebServer& server, WifiManager& wifiManager, ConfigManager& configManager);

  void handle();

  // --- NEW: Observer method implementation ---
  void onWifiStateChanged(WifiManager::State newState) override;

private:
  void begin();
  void stop();
  void setupRoutes();
  void sendStatusJson(AsyncWebServerRequest* request) const;
  void sendNetworksJson(AsyncWebServerRequest* request) const;
  String getSavedCredentialsJson() const;
  String templateProcessor(const String& var);

  AsyncWebServer& m_server;
  WifiManager& m_wifiManager;
  ConfigManager& m_configManager;
  DNSServer m_dnsServer;

  PortalStatus m_portalStatus = PortalStatus::IDLE;
  IntervalTimer m_portalTestTimer;
  IntervalTimer m_rebootTimer;
  bool m_reboot_scheduled = false;
  bool m_isRunning = false;  // Internal state
};

#endif  // PORTAL_SERVER_H