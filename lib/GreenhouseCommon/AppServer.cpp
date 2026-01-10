#include "AppServer.h"

#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <Updater.h>

#include "WebAppData.h"
#include "constants.h"
#include "node_config.h"
#include "utils.h"
#include "Logger.h"

AppServer::AppServer(AsyncWebServer& server, AsyncWebSocket& ws, ConfigManager& configManager)
    : m_server(server), m_ws(ws), m_configManager(configManager) {}

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
    // Ini adalah "jantung" mDNS. Tanpa ini, ESP8266 seringkali
    // tidak membalas query "gh-1-node-4.local" dari komputer.
    MDNS.update();

    if (m_rebootRequired && millis() - m_rebootTimestamp > 3000) {
      LOG_INFO("APP", F("Graceful rebooting now..."));
      ESP.restart();
    }
  }
}

void AppServer::begin() {
  if (m_isRunning)
    return;
  stop();
  setupRoutes();
  m_server.begin();

  String hostname = m_configManager.getHostname();

  // Memulai mDNS responder
  if (MDNS.begin(hostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
    LOG_INFO("mDNS", F("Responder started: http://%s.local"), hostname.c_str());
  } else {
    LOG_ERROR("mDNS", F("Error setting up MDNS responder!"));
  }

  // ArduinoOTA juga diinisialisasi di sini karena terkait server
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.begin();
  m_isRunning = true;
}

void AppServer::stop() {
  if (!m_isRunning)
    return;
  m_server.reset();
  ArduinoOTA.end();  // Bersihkan OTA saat stop
  MDNS.close();      // Hentikan mDNS advertisement
  m_isRunning = false;
}

void AppServer::onFlashRequest(std::function<void()> fn) {
  m_flash_request_callback = fn;
}

void AppServer::setupRoutes() {
  m_server.addHandler(&m_ws);

  auto sendAsset = [](AsyncWebServerRequest* request, const uint8_t* data, size_t len, const char* mime, bool gzipped) {
    AsyncWebServerResponse* response = request->beginResponse_P(200, mime, data, len);
    if (gzipped) {
      response->addHeader("Content-Encoding", "gzip");
    }
    if (String(mime).startsWith("image/") || String(mime).endsWith("javascript") || String(mime).endsWith("css")) {
      response->addHeader("Cache-Control", "public, max-age=31536000");
    }
    request->send(response);
  };

  m_server.on("/", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, INDEX_HTML, INDEX_HTML_LEN, INDEX_HTML_MIME, INDEX_HTML_GZIPPED);
  });

  m_server.on("/crypto.js", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, CRYPTO_JS, CRYPTO_JS_LEN, CRYPTO_JS_MIME, CRYPTO_JS_GZIPPED);
  });

  m_server.on("/update", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, UPDATE_HTML, UPDATE_HTML_LEN, UPDATE_HTML_MIME, UPDATE_HTML_GZIPPED);
  });

  // Terminal page (WebSocket-based CLI)
  m_server.on("/terminal", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, TERMINAL_HTML, TERMINAL_HTML_LEN, TERMINAL_HTML_MIME, TERMINAL_HTML_GZIPPED);
  });

  // API endpoint for dashboard (JSON status)
  m_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->print("{");
    response->printf("\"firmware\":\"%s\",", FIRMWARE_VERSION);
    response->printf("\"nodeId\":\"%d-%d\",", GH_ID, NODE_ID);
    response->printf("\"freeHeap\":%u,", ESP.getFreeHeap());
    response->printf("\"uptime\":\"%luh\",", millis() / 3600000UL);
    response->printf("\"ssid\":\"%s\",", WiFi.SSID().c_str());
    response->printf("\"ip\":\"%s\"", WiFi.localIP().toString().c_str());
    response->print("}");
    request->send(response);
  });

  m_server.on(
      "/update",
      HTTP_POST,
      [](AsyncWebServerRequest* request) {},
      [this](
          AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
        if (index == 0) {
          LOG_INFO("WEB-OTA", F("Start. File: %s, Size: %u"), filename.c_str(), request->contentLength());

          unsigned long now = millis();

          // Simple Global Lockout (Anti-DoS)
          if (m_ota_fail_count >= AppConstants::MAX_FAILED_AUTH_ATTEMPTS) {
            if (now - m_ota_lockout_ts < AppConstants::AUTH_LOCKOUT_DURATION_MS) {
              request->send(429, "text/plain", "Too Many Requests (System Locked)");
              return;
            } else {
              // Lockout expired
              m_ota_fail_count = 0;
            }
          }

          char hashed_pass_buffer[65];
          const char* passArg = "";
          if (request->hasParam("password", true)) {
            passArg = request->getParam("password", true)->value().c_str();
          }

          // Hash is stored via output parameter; length return intentionally discarded
          (void)Utils::hash_sha256(std::span<char>(hashed_pass_buffer), passArg);

          if (strcmp(hashed_pass_buffer, this->m_configManager.getConfig().ADMIN_PASSWORD.data()) != 0) {
            LOG_WARN("WEB-OTA", F("Auth FAILED."));
            m_ota_fail_count++;
            m_ota_lockout_ts = now;
            request->send(401, "text/plain", "Auth Failed");
            return;
          }

          // Success
          m_ota_fail_count = 0;

          size_t content_len = request->contentLength();
          FSInfo fs_info;
          LittleFS.info(fs_info);
          if (fs_info.totalBytes - fs_info.usedBytes < content_len) {
            request->send(413, "text/plain", "Not enough filesystem space");
            return;
          }

          if (!Update.begin(content_len, U_FLASH)) {
            LOG_ERROR("WEB-OTA", F("Update Start Failed: %d"), Update.getError());
            request->send(500, "text/plain", "Update Start Failed");
            return;
          }
        }

        if (len > 0) {
          if (!Update.isRunning()) {
            return;
          }
          if (Update.write(data, len) != len) {
            LOG_ERROR("WEB-OTA", F("Write Failed: %d"), Update.getError());
            request->send(500, "text/plain", "Write Failed");
            return;
          }
        }

        if (final) {
          if (Update.end(true)) {
            LOG_INFO("WEB-OTA", F("Success. Total: %u bytes"), index + len);
            request->send(200, "text/plain", "Success! Rebooting...");
            // Defer reboot to allow response to flush
            m_rebootRequired = true;
            m_rebootTimestamp = millis();
            // delay(500); // Removed buffering delay
            // ESP.restart(); // Removed immediate restart
          } else {
            LOG_ERROR("WEB-OTA", F("Update End Failed: %d"), Update.getError());
            request->send(500, "text/plain", "Update End Failed");
          }
        }
      });
}