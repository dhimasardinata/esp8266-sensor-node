#include "AppServer.h"

#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Updater.h>

#include <cstring>

#include "ConfigManager.h"
#include "Logger.h"
#include "SensorManager.h"
#include "WebAppData.h"
#include "REDACTED"
#include "REDACTED"
#include "constants.h"
#include "node_config.h"
#include "sensor_data.h"
#include "utils.h"

// AppServer.Routes.cpp - HTTP routes and OTA handling

namespace {
  bool endsWith(const char* str, const char* suffix) {
    if (!str || !suffix)
      return false;
    // MIME strings are short; cap scan to avoid worst-case walks.
    constexpr size_t kMaxMimeLen = 64;
    size_t len = strnlen(str, kMaxMimeLen);
    size_t slen = strnlen(suffix, kMaxMimeLen);
    if (slen > len)
      return false;
    return strncmp(str + (len - slen), suffix, slen) == 0;
  }
}

void AppServer::setupRoutes() {
  m_handlerCount = 0;
  setupStaticRoutes();
  setupWifiRoutes();
  setupOtaRoute();
}

bool AppServer::storeHandler(AsyncWebHandler* handler) {
  constexpr size_t kMaxHandlers = sizeof(m_handlers) / sizeof(m_handlers[0]);
  if (m_handlerCount < kMaxHandlers) {
    m_handlers[m_handlerCount++] = handler;
    return true;
  }
  LOG_ERROR("APP", F("Handler table full; route skipped"));
  return false;
}


void AppServer::setupStaticRoutes() {
  auto sendAsset = [](AsyncWebServerRequest* request, const uint8_t* data, size_t len, const char* mime, bool gzipped) {
    AsyncWebServerResponse* response = request->beginResponse_P(200, mime, data, len);
    if (gzipped)
      response->addHeader("Content-Encoding", "gzip");
    if (strncmp(mime, "image/", 6) == 0 || endsWith(mime, "javascript") || endsWith(mime, "css")) {
      response->addHeader("Cache-Control", "public, max-age=31536000");
    }
    request->send(response);
  };

  storeHandler(&(m_server.on("/", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, INDEX_HTML, INDEX_HTML_LEN, INDEX_HTML_MIME, INDEX_HTML_GZIPPED);
  })));

  storeHandler(&(m_server.on("/crypto.js", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, CRYPTO_JS, CRYPTO_JS_LEN, CRYPTO_JS_MIME, CRYPTO_JS_GZIPPED);
  })));

  storeHandler(&(m_server.on("/update", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, UPDATE_HTML, UPDATE_HTML_LEN, UPDATE_HTML_MIME, UPDATE_HTML_GZIPPED);
  })));

  storeHandler(&(m_server.on("/terminal", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, TERMINAL_HTML, TERMINAL_HTML_LEN, TERMINAL_HTML_MIME, TERMINAL_HTML_GZIPPED);
  })));

  storeHandler(&(m_server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleStatusRequest(request);
  })));

  // WiFi saved credentials endpoint
  storeHandler(&(m_server.on("REDACTED", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleWifiSavedRequest(request);
  })));
}


void AppServer::setupWifiRoutes() {
  storeHandler(&(m_server.on("/networks", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleNetworksRequest(request);
  })));

  storeHandler(&(m_server.on("/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleSaveRequest(request);
  })));

  storeHandler(&(m_server.on("/forget", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleForgetRequest(request);
  })));
}


void AppServer::setupOtaRoute() {
  storeHandler(&(m_server.on(
      "/update",
      HTTP_POST,
      [](AsyncWebServerRequest* request) {},
      [this](
          AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
        handleOtaUpload(request, filename, index, data, len, final);
      })));
}

void AppServer::handleStatusRequest(AsyncWebServerRequest* request) {
  if (ESP.getFreeHeap() < 2500) {
    request->send(503, "application/json", "{\"error\":\"Low memory\"}");
    return;
  }
  AsyncResponseStream* response = request->beginResponseStream("application/json", 512);
  IPAddress ip = WiFi.localIP();

  // Get sensor readings
  SensorReading temp = m_sensorManager.getTemp();
  SensorReading hum = m_sensorManager.getHumidity();
  SensorReading light = m_sensorManager.getLight();

  // Escape SSID to prevent JSON injection (uses stack buffer)
  char ssidRaw[WIFI_SSID_MAX_LEN] = REDACTED
  WiFi.SSID().toCharArray(ssidRaw, sizeof(ssidRaw));
  char safeSsid[68];
  (void)Utils::escape_json_string(std::span{safeSsid}, ssidRaw);

  // CHANGED: Escape Firmware Version to prevent injection via build flags
  char safeFw[32];
  (void)Utils::escape_json_string(std::span{safeFw}, FIRMWARE_VERSION);

  response->printf(
      "{\"firmware\":\"%s\",\"nodeId\":\"%d-%d\",\"freeHeap\":%u,\"uptime\":\"%luh\","
      "\"ssid\":REDACTED
      "\"temperature\":%.1f,\"humidity\":%.1f,\"lux\":%u,"
      "\"tempValid\":%s,\"humValid\":%s,\"luxValid\":%s}",
      safeFw,
      GH_ID,
      NODE_ID,
      ESP.getFreeHeap(),
      millis() / 3600000UL,
      safeSsid,
      ip[0],
      ip[1],
      ip[2],
      ip[3],
      temp.isValid ? temp.value : 0.0f,
      hum.isValid ? hum.value : 0.0f,
      light.isValid ? (uint16_t)light.value : 0,
      temp.isValid ? "true" : "false",
      hum.isValid ? "true" : "false",
      light.isValid ? "true" : "false");
  request->send(response);
}

void AppServer::handleWifiSavedRequest(AsyncWebServerRequest* request) {
  if (ESP.getFreeHeap() < 2500) {
    request->send(503, "application/json", "{\"error\":\"Low memory\"}");
    return;
  }
  AsyncResponseStream* response = request->beginResponseStream("application/json", 1024);
  auto& store = m_wifiManager.getCredentialStore();

  response->print("{");

  // Primary GH
  response->print("\"primary\":");
  auto* primary = store.getPrimaryGH();
  if (primary && !primary->isEmpty()) {
    char safePrimary[68];
    (void)Utils::escape_json_string(std::span{safePrimary}, primary->ssid);
    response->printf("{\"ssid\":REDACTED
                     safePrimary,
                     primary->lastRssi,
                     primary->isAvailable() ? "true" : "false");
  } else {
    response->print("null");
  }

  // Secondary GH
  response->print(",\"secondary\":");
  auto* secondary = store.getSecondaryGH();
  if (secondary && !secondary->isEmpty()) {
    char safeSecondary[68];
    (void)Utils::escape_json_string(std::span{safeSecondary}, secondary->ssid);
    response->printf("{\"ssid\":REDACTED
                     safeSecondary,
                     secondary->lastRssi,
                     secondary->isAvailable() ? "true" : "false");
  } else {
    response->print("null");
  }

  // User saved credentials
  response->print(",\"saved\":[");
  auto saved = store.getSavedCredentials();
  bool first = true;
  for (auto& cred : saved) {
    if (!cred.isEmpty()) {
      if (!first)
        response->print(",");
      first = false;
      char safeSsid[68];
      (void)Utils::escape_json_string(std::span{safeSsid}, cred.ssid);
      response->printf("{\"ssid\":REDACTED
                       safeSsid,
                       cred.lastRssi,
                       cred.isAvailable() ? "true" : "false",
                       cred.isHidden() ? "true" : "false");
    }
  }
  response->print("]}");

  store.releaseSavedCredentials();
  request->send(response);
}

void AppServer::handleNetworksRequest(AsyncWebServerRequest* request) {
  if (ESP.getFreeHeap() < 4500 || ESP.getMaxFreeBlockSize() < 2500) {
    request->send(503, "application/json", "{\"error\":\"Low memory\"}");
    return;
  }

  unsigned long now = millis();
  if (now - m_lastScanRequest < 2000) {
    request->send(429, "application/json", "{\"error\":\"Rate limited\"}");
    return;
  }
  m_lastScanRequest = now;

  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    request->send(200, "application/json", "{\"scanning\":true}");
    return;
  }

  if (n < 0) {
    // Trigger non-blocking scan via WifiManager to avoid WDT in async context.
    m_wifiManager.requestPortalScan();
    request->send(200, "application/json", "{\"scanning\":true}");
    return;
  }

  WifiManager:REDACTED
  uint8_t count = m_wifiManager.copyScanResults(results, WifiManager::MAX_SCAN_RESULTS);
  if (count == 0) {
    // If no cached results, request a scan and return scanning status.
    m_wifiManager.requestPortalScan();
    request->send(200, "application/json", "{\"scanning\":true}");
    return;
  }

  AsyncResponseStream* response = request->beginResponseStream("application/json", 512);
  response->print("{\"networks\":[");

  bool first = true;
  auto& store = m_wifiManager.getCredentialStore();
  for (uint8_t i = 0; i < count; ++i) {
    const char* ssid = REDACTED
    if (!ssid || ssid[0] =REDACTED
      continue;
    int32_t rssi = results[i].rssi;
    bool isOpen = results[i].isOpen;
    bool isKnown = store.hasCredential(ssid);
    WifiRouteUtils:REDACTED
  }

  response->print("]}");
  store.releaseSavedCredentials();
  request->send(response);
}

void AppServer::handleSaveRequest(AsyncWebServerRequest* request) {
  if (ESP.getFreeHeap() < 6000) {
    request->send(503, "application/json", "{\"status\":\"error\",\"message\":\"Low memory\"}");
    return;
  }

  char ssid[33] = REDACTED
  char pass[65] = REDACTED
  bool hidden = request->hasArg("hidden");

  if (!request->hasArg("REDACTED")) {
    request->send_P(400, "REDACTED", PSTR("REDACTED"));
    return;
  }

  request->arg("REDACTED").toCharArray(ssid, sizeof(ssid));

  if (request->hasArg("REDACTED")) {
    request->arg("REDACTED").toCharArray(pass, sizeof(pass));
  }

  size_t ssidLen = REDACTED
  size_t passLen = REDACTED

  if (ssidLen =REDACTED
    request->send_P(400, "text/plain", PSTR("Invalid Input"));
    return;
  }
  if (!Utils::isSafeString(std::string_view(ssid, ssidLen)) ||
      !Utils::isSafeString(std::string_view(pass, passLen))) {
    request->send_P(400, "text/plain", PSTR("Invalid Characters"));
    return;
  }

  if (m_wifiManager.addUserCredential(std:REDACTED
                                      std::string_view(pass, passLen),
                                      hidden)) {
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save\"}");
  }
}

void AppServer::handleForgetRequest(AsyncWebServerRequest* request) {
  if (ESP.getFreeHeap() < 6000) {
    request->send(503, "application/json", "{\"status\":\"error\",\"message\":\"Low memory\"}");
    return;
  }

  char ssid[33] = REDACTED
  if (request->hasArg("REDACTED")) {
    request->arg("REDACTED").toCharArray(ssid, sizeof(ssid));
  }
  size_t ssidLen = REDACTED
  if (ssidLen > 0 && m_wifiManager.removeUserCredential(std:REDACTED
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    request->send(400, "application/json", "{\"status\":\"error\"}");
  }
}

// =============================================================================
// OTA Upload Helper Methods
// =============================================================================


bool AppServer::handleOtaInit(AsyncWebServerRequest* request, const String& filename) {
  LOG_INFO("WEB-OTA", F("Start. File: REDACTED

  unsigned long now = millis();

  // Global Lockout (Anti-DoS).
  if (m_ota_fail_count >= REDACTED
    if (now - m_ota_lockout_ts < AppConstants:REDACTED
      request->send(429, "text/plain", "Too Many Requests (System Locked)");
      return false;
    }
    m_ota_fail_count = REDACTED
  }

  // Authentication check
  char hashed_pass_buffer[65];
  char passArg[MAX_PASS_LEN] = REDACTED
  if (request->hasParam("REDACTED", true)) {
    const String passStr = REDACTED
    passStr.toCharArray(passArg, sizeof(passArg));
  }

  size_t passLen = REDACTED
  (void)Utils::hash_sha256(std::span<char>(hashed_pass_buffer), std::string_view(passArg, passLen));

  if (!Utils::consttime_equal(hashed_pass_buffer, m_configManager.getAdminPassword(), 64)) {
    LOG_WARN("REDACTED", F("REDACTED"));
    m_ota_fail_count++;
    m_ota_lockout_ts = REDACTED
    m_configManager.releaseStrings();
    request->send(401, "REDACTED", "REDACTED");
    return false;
  }

  m_configManager.releaseStrings();
  m_ota_fail_count = REDACTED

  // Check filesystem space
  size_t content_len = request->contentLength();
  FSInfo fs_info;
  LittleFS.info(fs_info);
  if (fs_info.totalBytes - fs_info.usedBytes < content_len) {
    request->send(413, "text/plain", "Not enough filesystem space");
    return false;
  }

  // Enable Async mode to prevent Update.write() from yielding in SYS context
  Update.runAsync(true);

  if (!Update.begin(content_len, U_FLASH)) {
    LOG_ERROR("WEB-OTA", F("Update Start Failed: REDACTED
    request->send(500, "text/plain", "Update Start Failed");
    return false;
  }

  if (m_otaStartCallback)
    m_otaStartCallback();
  return true;
}


bool AppServer::handleOtaWrite(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
  if (!Update.isRunning())
    return false;

  if (Update.write(data, len) != len) {
    LOG_ERROR("WEB-OTA", F("Write Failed: REDACTED
    request->send(500, "text/plain", "Write Failed");
    if (m_otaEndCallback)
      m_otaEndCallback();  // Resume on failure
    return false;
  }
  return true;
}


void AppServer::handleOtaFinalize(AsyncWebServerRequest* request, size_t totalSize) {
  if (Update.end(true)) {
    LOG_INFO("WEB-OTA", F("Success. Total: REDACTED
    request->send(200, "text/plain", "Success! Rebooting...");
    m_rebootRequired = true;
    m_rebootTimestamp = millis();
    if (m_otaEndCallback)
      m_otaEndCallback();
  } else {
    LOG_ERROR("WEB-OTA", F("Update End Failed: REDACTED
    request->send(500, "text/plain", "Update End Failed");
    if (m_otaEndCallback)
      m_otaEndCallback();
  }
}

// =============================================================================
// Main OTA Upload Handler
// =============================================================================


void AppServer::handleOtaUpload(
    AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
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
