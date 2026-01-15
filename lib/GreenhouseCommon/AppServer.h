// include/AppServer.h

#ifndef APP_SERVER_H
#define APP_SERVER_H

#include <functional>
#include "ConfigManager.h"
#include "IWifiStateObserver.h"

class AsyncWebServer;
class AsyncWebSocket;
class AsyncWebHandler;
class ISensorManager;

class AppServer : public IWifiStateObserver {
public:
  AppServer(AsyncWebServer& server, AsyncWebSocket& ws, ConfigManager& configManager, ISensorManager& sensorManager);

  // Disable copy
  AppServer(const AppServer&) = delete;
  AppServer& operator=(const AppServer&) = delete;

  void onFlashRequest(std::function<void()> fn);

  // Observer method implementation
  void onWifiStateChanged(WifiManager::State newState) override;
  void handle();

private:
  void begin();
  void stop();
  void setupRoutes();
  void setupStaticRoutes();
  void setupOtaRoute();
  void handleOtaUpload(class AsyncWebServerRequest* request, const String& filename, 
                       size_t index, uint8_t* data, size_t len, bool final);
  
  // OTA helper methods (reduce complexity)
  bool handleOtaInit(class AsyncWebServerRequest* request, const String& filename);
  bool handleOtaWrite(class AsyncWebServerRequest* request, uint8_t* data, size_t len);
  void handleOtaFinalize(class AsyncWebServerRequest* request, size_t totalSize);

  AsyncWebServer& m_server;
  AsyncWebSocket& m_ws;
  ConfigManager& m_configManager;
  ISensorManager& m_sensorManager;

  std::function<void()> m_flash_request_callback;

  uint32_t m_ota_fail_count = 0;
  unsigned long m_ota_lockout_ts = 0;

  bool m_rebootRequired = false;
  unsigned long m_rebootTimestamp = 0;

  bool m_isRunning = false;
  
  AsyncWebHandler* m_handlers[10];
  uint8_t m_handlerCount = 0;
};

#endif  // APP_SERVER_H
