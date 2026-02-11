#include <Arduino.h>  // Build Hardening Complete
#include <ESPAsyncWebServer.h>
#include <memory>
#include <WiFiClientSecureBearSSL.h>

#include "ApiClient.h"
#include "AppServer.h"
#include "Application.h"
#include "BootManager.h"
#include "BootGuard.h"
#include "CacheManager.h"
#include "ConfigManager.h"
#include "DiagnosticsTerminal.h"
#include "HAL.h"
#include "Logger.h"
#include "NtpClient.h"
#include "REDACTED"
#include "PortalServer.h"
#include "REDACTED"
#include "SensorManager.h"
#include "REDACTED"
#include "constants.h"

// Reserve heap to be released during portal mode (keep portal HTTP requests alive).
// Disable heap reserve to maximize available heap for TLS/HTTP.
constexpr size_t PORTAL_HEAP_RESERVE_MAX = 0;
constexpr size_t PORTAL_HEAP_RESERVE_MIN = 0;
constexpr size_t PORTAL_HEAP_RESERVE_STEP = 0;
constexpr size_t PORTAL_HEAP_RESERVE_MIN_FREE = 0;
constexpr size_t PORTAL_HEAP_RESERVE_MIN_BLOCK = 0;


namespace {
  class WifiMemoryModeObserver final : REDACTED
  public:
    WifiMemoryModeObserver(AsyncWebSocket& ws,
                           ConfigManager& configManager,
                           BearSSL::WiFiClientSecure& client,
                           ApiClient& apiClient,
                           OtaManager& otaManager,
                           DiagnosticsTerminal& terminal)
        : m_ws(ws),
          m_configManager(configManager),
          m_client(client),
          m_apiClient(apiClient),
          m_otaManager(otaManager),
          m_terminal(terminal),
          m_heapReserve(nullptr) {
    }

    void onWifiStateChanged(WifiManager:REDACTED
      if (newState == WifiManager::State::PORTAL_MODE) {
        if (m_mode != Mode::PORTAL) {
          releaseReserve();
          m_apiClient.pause();
          m_client.stop();
          m_client.setBufferSizes(AppConstants::TLS_RX_BUF_PORTAL, AppConstants::TLS_TX_BUF_PORTAL);
          m_client.setInsecure();
          m_mode = Mode::PORTAL;
          LOG_INFO("WIFI", F("Portal heap: REDACTED
                   ESP.getFreeHeap(),
                   ESP.getMaxFreeBlockSize());
          LOG_WARN("REDACTED", F("REDACTED"));
        }
      } else if (newState == WifiManager::State::CONNECTED_STA) {
        if (m_mode != Mode::CONNECTED) {
          m_client.stop();
          m_client.setBufferSizes(AppConstants::TLS_RX_BUF_PORTAL, AppConstants::TLS_TX_BUF_PORTAL);
          allocateReserve();
          const bool terminalReady = m_terminal.setEnabled(true);
          m_ws.enable(terminalReady);
          if (!terminalReady) {
            LOG_WARN("REDACTED", F("REDACTED"));
          }
          m_apiClient.resume();
          m_mode = Mode::CONNECTED;
          LOG_INFO("REDACTED", F("REDACTED"));
          LOG_INFO("REDACTED", F("REDACTED"));
        }
      }
    }

  private:
    void allocateReserve() {
      if (PORTAL_HEAP_RESERVE_MAX == 0) {
        return;
      }
      if (!m_heapReserve) {
        if (ESP.getFreeHeap() < PORTAL_HEAP_RESERVE_MIN_FREE ||
            ESP.getMaxFreeBlockSize() < PORTAL_HEAP_RESERVE_MIN_BLOCK) {
          LOG_WARN("REDACTED",
                   F("Heap reserve skipped (free=%u, block=%u)"),
                   ESP.getFreeHeap(),
                   ESP.getMaxFreeBlockSize());
          return;
        }
        size_t target = PORTAL_HEAP_RESERVE_MAX;
        while (target >= PORTAL_HEAP_RESERVE_MIN) {
          m_heapReserve.reset(new (std::nothrow) uint8_t[target]);
          if (m_heapReserve) {
            m_heapReserveSize = target;
            LOG_INFO("WIFI", F("Heap reserve allocated: REDACTED
            return;
          }
          if (target <= PORTAL_HEAP_RESERVE_STEP) {
            break;
          }
          target -= PORTAL_HEAP_RESERVE_STEP;
        }
        LOG_WARN("REDACTED", F("REDACTED"));
      }
    }

    void releaseReserve() {
      if (m_heapReserve) {
        m_heapReserve.reset();
        LOG_INFO("WIFI", F("Heap reserve released: REDACTED
        m_heapReserveSize = 0;
      }
    }

    enum class Mode { UNKNOWN, PORTAL, CONNECTED };
    Mode m_mode = Mode::UNKNOWN;
    AsyncWebSocket& m_ws;
    ConfigManager& m_configManager;
    BearSSL::WiFiClientSecure& m_client;
    ApiClient& m_apiClient;
    OtaManager& m_otaManager;
    DiagnosticsTerminal& m_terminal;
    std::unique_ptr<uint8_t[]> m_heapReserve;
    size_t m_heapReserveSize = 0;
  };

  struct Runtime {
    // Core IO
    AsyncWebServer server;
    AsyncWebSocket ws;

    // Configuration + Security
    ConfigManager configManager;
    BearSSL::WiFiClientSecure secureClient;

    // Core services
    SensorManager sensorManager;
    WifiManager wifiManager;  // WifiManager needs config later in init()
    CacheManager cacheManager;
    NtpClient ntpClient;

    AppServer appServer;
    PortalServer portalServer;
    ApiClient apiClient;
    OtaManager otaManager;
    ApplicationServices appServices;

    TerminalServices terminalServices;

    DiagnosticsTerminal terminal;
    WifiMemoryModeObserver wifiMemObserver;
    Application app;

    Runtime()
        : server(80),
          ws("/ws"),
          configManager(),
          secureClient(),
          sensorManager(),
          wifiManager(),
          cacheManager(),
          ntpClient(wifiManager),
          appServer(server, ws, configManager, sensorManager, wifiManager),
          portalServer(server, wifiManager, configManager),
          apiClient(ws, ntpClient, wifiManager, sensorManager, secureClient, configManager, cacheManager, nullptr),
          otaManager(ntpClient, wifiManager, secureClient, configManager, nullptr),
          appServices{configManager,
                      wifiManager,
                      ntpClient,
                      sensorManager,
                      apiClient,
                      otaManager,
                      appServer,
                      portalServer},
          terminalServices{configManager,
                           wifiManager,
                           ntpClient,
                           sensorManager,
                           cacheManager,
                           apiClient,
                           otaManager},
          terminal(ws, terminalServices),
          wifiMemObserver(ws, configManager, secureClient, apiClient, otaManager, terminal),
          app(appServices) {}

    void init() {
      // Lifecycle contract: config -> SSL -> servers -> observers -> init -> app loop
      configManager.init();

      if (configManager.getConfig().ALLOW_INSECURE_HTTPS()) {
        LOG_WARN("SEC", F("WARNING: HTTPS Validation Disabled by Config!"));
        secureClient.setInsecure();
      }
      // Start with portal-safe buffers; upgraded when STA connects.
      secureClient.setBufferSizes(AppConstants::TLS_RX_BUF_PORTAL, AppConstants::TLS_TX_BUF_PORTAL);
      secureClient.setTimeout(15000);

      // FIX: Setup server BEFORE creating handlers that depend on it
      // Add WebSocket handler before starting server
      server.addHandler(&ws);
      portalServer.preinitRoutes();

      // FIX: Start server AFTER all handlers are registered
      server.begin();

      appServices.setTerminal(&terminal);

      wifiManager.registerObserver(&portalServer);
      wifiManager.registerObserver(&appServer);
      wifiManager.registerObserver(&ntpClient);
      wifiManager.registerObserver(&wifiMemObserver);
      configManager.registerObserver(&app);

      cacheManager.init();
      sensorManager.init();
      wifiManager.init(configManager);
      ntpClient.init();
      apiClient.init();
      otaManager.init();

      // Wire up OTA callbacks for memory safety
      appServer.setOtaCallbacks(
          [this]() {
            sensorManager.pause();
            apiClient.pause();
          },
          [this]() {
            sensorManager.resume();
            apiClient.resume();
          });

      terminal.init();
      app.init();
    }
  };

  Runtime* g_runtime = nullptr;
}  // namespace

void setup() {
  delay(1000);
  static SerialManager serial;

  BootManager::run();

  // --- NORMAL BOOT SEQUENCE ---
  static Runtime runtime;
  runtime.init();
  g_runtime = &runtime;

  LOG_INFO("BOOT", F("Setup Complete - Entering Main Loop"));
}

void loop() {
  // FIX: Add null safety check with error logging
  if (!g_runtime) {
    LOG_ERROR("LOOP", F("CRITICAL: Application object is NULL!"));
    delay(1000);
    ESP.restart();
    return;
  }

  // Bersihkan klien WS yang disconnect untuk hemat RAM
  g_runtime->ws.cleanupClients();

  g_runtime->app.loop();

  // HEAP GUARD: Proactive yield to allow network stack to breathe
  static unsigned long lastYield = 0;
  unsigned long now = millis();

  // FIX: Handle millis() overflow
  if (now - lastYield > 100 || now < lastYield) {
    yield();
    lastYield = now;
  }

  // FIX: Mark stable and clear crash counter after successful runtime
  static bool markedStable = false;
  if (!markedStable && millis() > 60000) {
    BootGuard::markStable();
    BootGuard::clear();  // FIX: Clear counter after stable operation confirmed
    markedStable = true;
    LOG_INFO("BOOT", F("System marked as stable - crash counter cleared"));
  }
}
