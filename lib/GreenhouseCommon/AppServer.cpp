#include "AppServer.h"
#include <ESPAsyncWebServer.h>

#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <Updater.h>

#include "ISensorManager.h"
#include "WebAppData.h"
#include "constants.h"
#include "node_config.h"
#include "sensor_data.h"
#include "utils.h"
#include "Logger.h"

AppServer::AppServer(AsyncWebServer& server, AsyncWebSocket& ws, ConfigManager& configManager, ISensorManager& sensorManager)
    : m_server(server), m_ws(ws), m_configManager(configManager), m_sensorManager(sensorManager) {}

void AppServer::onWifiStateChanged(WifiManager::State newState) {
  if (newState == WifiManager::State::CONNECTED_STA) {
    if (!m_isRunning) {
      LOG_INFO("APP", F("WiFi Connected. Starting AppServer."));
      begin();
    }
  } else {
    if (m_isRunning) {
      LOG_INFO("APP", F("WiFi Disconnected. Stopping AppServer."));
      stop();
    }
  }
}

// --- PERBAIKAN: Implementasi handle() ---
void AppServer::handle() {
  if (m_isRunning) {
    // HEAP GUARD: Throttle mDNS if memory is critically low to prevent OOM
    if (ESP.getMaxFreeBlockSize() > 8000) {
      MDNS.update();
    }

    if (m_rebootRequired && millis() - m_rebootTimestamp > 3000) {
      LOG_INFO("APP", F("Graceful rebooting now..."));
      ESP.restart();
    }
  }
}

void AppServer::begin() {
  if (m_isRunning)
    return;
  setupRoutes();

  char hostname[32];
  m_configManager.getHostname(hostname, sizeof(hostname));

  // Memulai mDNS responder
  if (MDNS.begin(hostname)) {
    MDNS.addService("http", "tcp", 80);
    LOG_INFO("mDNS", F("Responder started: http://%s.local"), hostname);
  } else {
    LOG_ERROR("mDNS", F("Error setting up MDNS responder!"));
  }

  // ArduinoOTA juga diinisialisasi di sini karena terkait server
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.begin();
  m_isRunning = true;
}

void AppServer::stop() {
  if (!m_isRunning)
    return;
  for (size_t i = 0; i < m_handlerCount; ++i) {
    m_server.removeHandler(m_handlers[i]);
  }
  m_handlerCount = 0;
  ArduinoOTA.end();  // Bersihkan OTA saat stop
  MDNS.close();      // Hentikan mDNS advertisement
  m_isRunning = false;
}

void AppServer::onFlashRequest(std::function<void()> fn) {
  m_flash_request_callback = fn;
}

void AppServer::setupRoutes() {
  m_handlerCount = 0;
  setupStaticRoutes();
  setupOtaRoute();
}

void AppServer::setupStaticRoutes() {
  auto sendAsset = [](AsyncWebServerRequest* request, const uint8_t* data, size_t len, const char* mime, bool gzipped) {
    AsyncWebServerResponse* response = request->beginResponse_P(200, mime, data, len);
    if (gzipped) response->addHeader("Content-Encoding", "gzip");
    if (String(mime).startsWith("image/") || String(mime).endsWith("javascript") || String(mime).endsWith("css")) {
      response->addHeader("Cache-Control", "public, max-age=31536000");
    }
    request->send(response);
  };

  m_handlers[m_handlerCount++] = &(m_server.on("/", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, INDEX_HTML, INDEX_HTML_LEN, INDEX_HTML_MIME, INDEX_HTML_GZIPPED);
  }));

  m_handlers[m_handlerCount++] = &(m_server.on("/crypto.js", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, CRYPTO_JS, CRYPTO_JS_LEN, CRYPTO_JS_MIME, CRYPTO_JS_GZIPPED);
  }));

  m_handlers[m_handlerCount++] = &(m_server.on("/update", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, UPDATE_HTML, UPDATE_HTML_LEN, UPDATE_HTML_MIME, UPDATE_HTML_GZIPPED);
  }));

  m_handlers[m_handlerCount++] = &(m_server.on("/terminal", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, TERMINAL_HTML, TERMINAL_HTML_LEN, TERMINAL_HTML_MIME, TERMINAL_HTML_GZIPPED);
  }));

  m_handlers[m_handlerCount++] = &(m_server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    IPAddress ip = WiFi.localIP();
    
    // Get sensor readings
    SensorReading temp = m_sensorManager.getTemp();
    SensorReading hum = m_sensorManager.getHumidity();
    SensorReading light = m_sensorManager.getLight();
    
    response->printf(
      "{\"firmware\":\"%s\",\"nodeId\":\"%d-%d\",\"freeHeap\":%u,\"uptime\":\"%luh\","
      "\"ssid\":\"%s\",\"ip\":\"%u.%u.%u.%u\","
      "\"temperature\":%.1f,\"humidity\":%.1f,\"lux\":%u,"
      "\"tempValid\":%s,\"humValid\":%s,\"luxValid\":%s}",
      FIRMWARE_VERSION, GH_ID, NODE_ID, ESP.getFreeHeap(), millis() / 3600000UL,
      WiFi.SSID().c_str(), ip[0], ip[1], ip[2], ip[3],
      temp.isValid ? temp.value : 0.0f,
      hum.isValid ? hum.value : 0.0f,
      light.isValid ? (uint16_t)light.value : 0,
      temp.isValid ? "true" : "false",
      hum.isValid ? "true" : "false",
      light.isValid ? "true" : "false");
    request->send(response);
  }));
}

void AppServer::setupOtaRoute() {
  m_handlers[m_handlerCount++] = &(m_server.on(
      "/update",
      HTTP_POST,
      [](AsyncWebServerRequest* request) {},
      [this](AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
        handleOtaUpload(request, filename, index, data, len, final);
      }));
}

// =============================================================================
// OTA Upload Helper Methods (Reduce Cyclomatic Complexity)
// =============================================================================

bool AppServer::handleOtaInit(AsyncWebServerRequest* request, const String& filename) {
  LOG_INFO("WEB-OTA", F("Start. File: %s, Size: %u"), filename.c_str(), request->contentLength());

  unsigned long now = millis();

  // Simple Global Lockout (Anti-DoS)
  if (m_ota_fail_count >= AppConstants::MAX_FAILED_AUTH_ATTEMPTS) {
    if (now - m_ota_lockout_ts < AppConstants::AUTH_LOCKOUT_DURATION_MS) {
      request->send(429, "text/plain", "Too Many Requests (System Locked)");
      return false;
    }
    m_ota_fail_count = 0;
  }

  // Authentication check
  char hashed_pass_buffer[65];
  const char* passArg = "";
  if (request->hasParam("password", true)) {
    passArg = request->getParam("password", true)->value().c_str();
  }

  (void)Utils::hash_sha256(std::span<char>(hashed_pass_buffer), passArg);

  if (strcmp(hashed_pass_buffer, m_configManager.getConfig().ADMIN_PASSWORD.data()) != 0) {
    LOG_WARN("WEB-OTA", F("Auth FAILED."));
    m_ota_fail_count++;
    m_ota_lockout_ts = now;
    request->send(401, "text/plain", "Auth Failed");
    return false;
  }

  m_ota_fail_count = 0;

  // Check filesystem space
  size_t content_len = request->contentLength();
  FSInfo fs_info;
  LittleFS.info(fs_info);
  if (fs_info.totalBytes - fs_info.usedBytes < content_len) {
    request->send(413, "text/plain", "Not enough filesystem space");
    return false;
  }

  // Begin update
  if (!Update.begin(content_len, U_FLASH)) {
    LOG_ERROR("WEB-OTA", F("Update Start Failed: %d"), Update.getError());
    request->send(500, "text/plain", "Update Start Failed");
    return false;
  }

  return true;
}

bool AppServer::handleOtaWrite(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
  if (!Update.isRunning()) return false;
  
  if (Update.write(data, len) != len) {
    LOG_ERROR("WEB-OTA", F("Write Failed: %d"), Update.getError());
    request->send(500, "text/plain", "Write Failed");
    return false;
  }
  return true;
}

void AppServer::handleOtaFinalize(AsyncWebServerRequest* request, size_t totalSize) {
  if (Update.end(true)) {
    LOG_INFO("WEB-OTA", F("Success. Total: %u bytes"), totalSize);
    request->send(200, "text/plain", "Success! Rebooting...");
    m_rebootRequired = true;
    m_rebootTimestamp = millis();
  } else {
    LOG_ERROR("WEB-OTA", F("Update End Failed: %d"), Update.getError());
    request->send(500, "text/plain", "Update End Failed");
  }
}

// =============================================================================
// Main OTA Upload Handler (Refactored)
// =============================================================================

void AppServer::handleOtaUpload(AsyncWebServerRequest* request, const String& filename, 
                                 size_t index, uint8_t* data, size_t len, bool final) {
  // Initialize on first chunk
  if (index == 0 && !handleOtaInit(request, filename)) {
    return;
  }

  // Write data chunks
  if (len > 0 && !handleOtaWrite(request, data, len)) {
    return;
  }

  // Finalize on last chunk
  if (final) {
    handleOtaFinalize(request, index + len);
  }
}