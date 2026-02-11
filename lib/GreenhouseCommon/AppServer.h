// include/AppServer.h

#ifndef APP_SERVER_H
#define APP_SERVER_H

#include <functional>
#include "REDACTED"

class AsyncWebServer;
class AsyncWebSocket;
class AsyncWebHandler;
class ConfigManager;
class SensorManager;  // Concrete type for CRTP
class WifiManager;    // For credential store access

class AppServer : public IWifiStateObserver {
public:
  AppServer(AsyncWebServer& server, AsyncWebSocket& ws, ConfigManager& configManager, 
            SensorManager& sensorManager, WifiManager& wifiManager);

  // Disable copy
  AppServer(const AppServer&) = delete;
  AppServer& operator=(const AppServer&) = delete;

  void onFlashRequest(std::function<void()> fn);
  void setOtaCallbacks(std:REDACTED

  // IWifiStateObserver implementation.
  void onWifiStateChanged(WifiManager:REDACTED
  void handle();

private:
  void begin();
  void stop();
  void setupRoutes();
  void setupStaticRoutes();
  void setupWifiRoutes();
  void setupOtaRoute();
  bool storeHandler(AsyncWebHandler* handler);
  void handleStatusRequest(class AsyncWebServerRequest* request);
  void handleWifiSavedRequest(class AsyncWebServerRequest* request);
  void handleNetworksRequest(class AsyncWebServerRequest* request);
  void handleSaveRequest(class AsyncWebServerRequest* request);
  void handleForgetRequest(class AsyncWebServerRequest* request);
  void handleOtaUpload(class AsyncWebServerRequest* request, const String& filename, 
                       size_t index, uint8_t* data, size_t len, bool final);
  
  // OTA Helper Methods.
  bool handleOtaInit(class AsyncWebServerRequest* request, const String& filename);
  bool handleOtaWrite(class AsyncWebServerRequest* request, uint8_t* data, size_t len);
  void handleOtaFinalize(class AsyncWebServerRequest* request, size_t totalSize);

  AsyncWebServer& m_server;
  AsyncWebSocket& m_ws;
  ConfigManager& m_configManager;
  SensorManager& m_sensorManager;
  WifiManager& m_wifiManager;

  std::function<void()> m_flash_request_callback;
  std::function<void()> m_otaStartCallback;
  std::function<void()> m_otaEndCallback;

  uint32_t m_ota_fail_count = REDACTED
  unsigned long m_ota_lockout_ts = REDACTED

  bool m_rebootRequired = false;
  unsigned long m_rebootTimestamp = 0;

  bool m_isRunning = false;
  
  AsyncWebHandler* m_handlers[16];
  uint8_t m_handlerCount = 0;
  unsigned long m_lastScanRequest = 0;
  unsigned long m_lastMdns = 0;
  unsigned long m_lastMdnsStartAttempt = 0;
  bool m_mdnsStarted = false;
};

#endif  // APP_SERVER_H
