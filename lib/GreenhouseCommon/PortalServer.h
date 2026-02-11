#ifndef PORTAL_SERVER_H
#define PORTAL_SERVER_H

#include <DNSServer.h>
#include <IntervalTimer.h>
#include <memory>

#include "ConfigManager.h"
#include "REDACTED"
#include "REDACTED"

struct CachedNetwork {
  int32_t rssi = 0;
  uint8_t bars = 0;
  bool isOpen = false;
  bool isKnown = false;
  char ssid[33] = REDACTED
};

static constexpr uint8_t MAX_CACHED_NETWORKS = 4;
static constexpr uint32_t SCAN_CACHE_DURATION_MS = 10000;

class AsyncWebServer;
class AsyncWebHandler;
class AsyncWebServerRequest;
namespace CryptoUtils {
  class AES_CBC_Cipher;
}

class PortalServer : public IWifiStateObserver {
public:
  enum class PortalStatus { IDLE, TESTING, SUCCESS, FAIL, DECRYPTION_FAIL };

  PortalServer(AsyncWebServer& server, WifiManager& wifiManager, ConfigManager& configManager);

  PortalServer(const PortalServer&) = delete;
  PortalServer& operator=(const PortalServer&) = delete;

  void handle();
  void onWifiStateChanged(WifiManager:REDACTED
  void preinitRoutes();

private:
  void begin();
  void stop();
  void setupRoutes();
  void handleSaveRequest(AsyncWebServerRequest* request);
  void handleScanRequest(AsyncWebServerRequest* request);
  void handleForgetRequest(AsyncWebServerRequest* request);
  void handleFactoryResetRequest(AsyncWebServerRequest* request);
  void sendStatusJson(AsyncWebServerRequest* request) const;
  void sendNetworksJson(AsyncWebServerRequest* request);
  void sendSavedCredentialsJson(AsyncWebServerRequest* request) const;
  void cacheNetworkScanResults();
  String templateProcessor(const String& var);

  // handle() helpers
  void handlePendingConnection();
  void startConnectionAttempt();
  void handleTestResult();


  AsyncWebServer& m_server;
  WifiManager& m_wifiManager;
  ConfigManager& m_configManager;
  DNSServer m_dnsServer;
  const CryptoUtils::AES_CBC_Cipher* const m_portalCipher;

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

  std::unique_ptr<CachedNetwork[]> m_cachedNetworks;
  uint8_t m_cachedNetworkCap = 0;
  uint8_t m_cachedNetworkCount = 0;
  uint32_t m_lastScanTime = 0;
  bool m_scanResultsCached = false;
  bool m_portalScanInProgress = false;
  uint32_t m_lastScanStart = 0;

  bool ensureCacheBuffer();
  void releaseCacheBuffer();

  AsyncWebHandler* m_handlers[12];
  uint8_t m_handlerCount = 0;
};

#endif  // PORTAL_SERVER_H
