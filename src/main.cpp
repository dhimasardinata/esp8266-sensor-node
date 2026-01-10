#include <Arduino.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFiClientSecureBearSSL.h>

#include "ApiClient.h"
#include "AppServer.h"
#include "Application.h"
#include "BootGuard.h"
#include "CacheManager.h"
#include "ConfigManager.h"
#include "CrashHandler.h"
#include "DiagnosticsTerminal.h"
#include "HAL.h"
#include "NtpClient.h"
#include "OtaManager.h"
#include "PortalServer.h"
#include "SensorManager.h"
#include "ServiceContainer.h"
#include "WifiManager.h"
#include "root_ca_data.h"
#include "Logger.h"

// --- KONFIGURASI BUFFER SSL ---
// FIX: Removed sizeof() wrapper which caused buffer to be only 4+128 bytes.
// Now it uses the actual value (384 + 128 = 512 bytes).
constexpr uint16_t SSL_TX_BUF_SIZE = MAX_PAYLOAD_SIZE + 128;
constexpr uint16_t SSL_RX_BUF_SIZE = 1024; // Increased from 768 for better compatibility

// --- GLOBAL OBJECTS ---
// Pindahkan server dan ws ke sini agar bisa diakses di loop()
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

static Application* app = nullptr;
static BearSSL::X509List trustedCerts(ROOT_CA_PEM);
static BearSSL::Session sslSession;

void setup() {
  delay(1000);
  static SerialManager serial;

  // 1. INPUT: Cek Crash Counter
  BootGuard::check();
  uint32_t crashes = BootGuard::getCrashCount();

  static FileSystemManager fileSystem;
  CrashHandler::process();

  // --- LOGIKA SELF-HEALING ---
  if (crashes >= 4 && crashes <= 7) {
    LOG_WARN("AUTO-FIX", F("Level 1 (Attempt %u): Clearing Sensor Cache..."), crashes);
    LittleFS.remove("/cache.dat");
    delay(100);
  } else if (crashes >= 8 && crashes <= 12) {
    LOG_ERROR("AUTO-FIX", F("Level 2 (Attempt %u): FORMATTING FILESYSTEM..."), crashes);
    if (LittleFS.format()) {
      LOG_INFO("AUTO-FIX", F("Format Success. Restarting to apply Factory Defaults."));
    } else {
      LOG_ERROR("AUTO-FIX", F("Format Failed! Hardware Issue?"));
    }
    delay(1000);
    ESP.restart();
  } else if (crashes > 12) {
    LOG_ERROR("AUTO-FIX", F("Level 3 (Attempt %u): System Unstable. Cooling down..."), crashes);
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    for (int i = 0; i < 10; i++) {
      ESP.wdtFeed();
    }
    LOG_INFO("AUTO-FIX", F("Retrying boot..."));
    if (crashes > 15)
      BootGuard::clear();
    ESP.restart();
  }

  // --- NORMAL BOOT SEQUENCE ---

  // 1. Initialize Config First (Needed for SSL decision)
  static ConfigManager configManager;
  configManager.init();

  // 2. Configure SSL based on Config
  static BearSSL::WiFiClientSecure secureClient;
  if (configManager.getConfig().ALLOW_INSECURE_HTTPS) {
       LOG_WARN("SEC", F("WARNING: HTTPS Validation Disabled by Config!"));
       secureClient.setInsecure();
  } else {
       secureClient.setTrustAnchors(&trustedCerts);
  }
  secureClient.setBufferSizes(SSL_RX_BUF_SIZE, SSL_TX_BUF_SIZE);
  secureClient.setSession(&sslSession);
  secureClient.setTimeout(15000);

  // 3. Initialize other services
  static SensorManager sensorManager;
  static WifiManager wifiManager; // WifiManager needs config later in init()
  static CacheManager cacheManager;
  static NtpClient ntpClient(wifiManager);

  // Pass global 'server' and 'ws' here
  static AppServer appServer(server, ws, configManager);
  static PortalServer portalServer(server, wifiManager, configManager);
  static ApiClient apiClient(ws, ntpClient, wifiManager, sensorManager, secureClient, configManager, cacheManager);
  static OtaManager otaManager(ntpClient, wifiManager, secureClient, configManager);

  static ServiceContainer services = {configManager,
                                      wifiManager,
                                      ntpClient,
                                      sensorManager,
                                      cacheManager,
                                      apiClient,
                                      otaManager,
                                      appServer,
                                      portalServer,
                                      secureClient};

  static DiagnosticsTerminal terminal(ws, services);
  services.setTerminal(&terminal);

  static Application main_app(services);
  app = &main_app;

  wifiManager.registerObserver(&services.appServer);
  wifiManager.registerObserver(&services.portalServer);
  wifiManager.registerObserver(&services.ntpClient);
  configManager.registerObserver(app);

  cacheManager.init();
  sensorManager.init();
  wifiManager.init(configManager);
  ntpClient.init();
  apiClient.init();
  otaManager.init();
  services.terminal->init();

  app->init();
}

void loop() {
  // Bersihkan klien WS yang disconnect untuk hemat RAM
  ws.cleanupClients();

  if (app)
    app->loop();

  static bool markedStable = false;
  if (!markedStable && millis() > 60000) {
    BootGuard::markStable();
    markedStable = true;
  }
}