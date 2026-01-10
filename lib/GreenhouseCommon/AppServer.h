// include/AppServer.h

#ifndef APP_SERVER_H
#define APP_SERVER_H

#include <ESPAsyncWebServer.h>

#include <functional>


#include "ConfigManager.h"
#include "IWifiStateObserver.h"

class AppServer : public IWifiStateObserver {
public:
  AppServer(AsyncWebServer& server, AsyncWebSocket& ws, ConfigManager& configManager);

  void onFlashRequest(std::function<void()> fn);

  // Observer method implementation
  void onWifiStateChanged(WifiManager::State newState) override;
  void handle();

private:
  void begin();
  void stop();
  void setupRoutes();

  AsyncWebServer& m_server;
  AsyncWebSocket& m_ws;
  ConfigManager& m_configManager;

  std::function<void()> m_flash_request_callback;

  uint32_t m_ota_fail_count = 0;
  unsigned long m_ota_lockout_ts = 0;

  bool m_rebootRequired = false;
  unsigned long m_rebootTimestamp = 0;

  bool m_isRunning = false;
};

#endif  // APP_SERVER_H