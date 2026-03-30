#include "web/AppServer.h"

#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Updater.h>

#include <cstring>

#include "system/ConfigManager.h"
#include "system/Logger.h"
#include "net/NtpClient.h"
#include "sensor/SensorManager.h"
#include "sensor/SensorNormalization.h"
#include "system/SystemHealth.h"
#include "generated/WebAppData.h"
#include "REDACTED"
#include "REDACTED"
#include "config/constants.h"
#include "generated/node_config.h"
#include "sensor/SensorData.h"
#include "support/Utils.h"

// AppServer.Routes.cpp - HTTP routes and OTA handling

namespace {
  void copy_trunc_P(char* out, size_t out_len, PGM_P text) {
    if (!out || out_len == 0) {
      return;
    }
    if (!text) {
      out[0] = '\0';
      return;
    }
    strncpy_P(out, text, out_len - 1);
    out[out_len - 1] = '\0';
  }

  void copy_json_bool(char* out, size_t out_len, bool value) {
    copy_trunc_P(out, out_len, value ? PSTR("true") : PSTR("false"));
  }

  void sendJsonResponse_P(AsyncWebServerRequest* request, int code, PGM_P body) {
    request->send_P(code, "application/json", body);
  }

  void sendTextResponse_P(AsyncWebServerRequest* request, int code, PGM_P body) {
    request->send_P(code, "text/plain", body);
  }

  bool endsWith_P(const char* str, PGM_P suffix) {
    if (!str || !suffix)
      return false;
    // MIME strings are short; cap scan to avoid worst-case walks.
    constexpr size_t kMaxMimeLen = 64;
    size_t len = strnlen(str, kMaxMimeLen);
    size_t slen = strlen_P(suffix);
    if (slen > kMaxMimeLen)
      slen = kMaxMimeLen;
    if (slen > len)
      return false;
    return strncmp_P(str + (len - slen), suffix, slen) == 0;
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
      response->addHeader(F("Content-Encoding"), F("gzip"));
    if (strncmp_P(mime, PSTR("image/"), 6) == 0 || endsWith_P(mime, PSTR("javascript")) ||
        endsWith_P(mime, PSTR("css"))) {
      response->addHeader(F("Cache-Control"), F("public, max-age=31536000"));
    }
    request->send(response);
  };

  storeHandler(&(m_server.on("/", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, INDEX_HTML, INDEX_HTML_LEN, INDEX_HTML_MIME, INDEX_HTML_GZIPPED);
  })));

  storeHandler(&(m_server.on("/crypto.js", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, CRYPTO_JS, CRYPTO_JS_LEN, CRYPTO_JS_MIME, CRYPTO_JS_GZIPPED);
  })));

  storeHandler(&(m_server.on("/terminal.css", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, TERMINAL_CSS, TERMINAL_CSS_LEN, TERMINAL_CSS_MIME, TERMINAL_CSS_GZIPPED);
  })));

  storeHandler(&(m_server.on("/terminal.js", HTTP_GET, [sendAsset](AsyncWebServerRequest* request) {
    sendAsset(request, TERMINAL_JS, TERMINAL_JS_LEN, TERMINAL_JS_MIME, TERMINAL_JS_GZIPPED);
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

  storeHandler(&(m_server.on("/api/time", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleTimeRequest(request);
  })));
  storeHandler(&(m_server.on("/api/time", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleTimeRequest(request);
  })));

  storeHandler(&(m_server.on("/api/fs/format", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleFormatFsRequest(request);
  })));

  // WiFi saved credentials endpoint
  storeHandler(&(m_server.on("REDACTED", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleWifiSavedRequest(request);
  })));
}

void AppServer::handleTimeRequest(AsyncWebServerRequest* request) {
  const char* argName = request->hasArg("epoch") ? "epoch" : (request->hasArg("t") ? "t" : nullptr);
  if (!argName) {
    sendJsonResponse_P(request, 400, PSTR("{\"status\":\"error\",\"msg\":\"missing\"}"));
    return;
  }

  const String& val = request->arg(argName);
  if (val.length() == 0) {
    sendJsonResponse_P(request, 400, PSTR("{\"status\":\"error\",\"msg\":\"empty\"}"));
    return;
  }

  char* endptr = nullptr;
  unsigned long long raw = strtoull(val.c_str(), &endptr, 10);
  if (endptr && *endptr != '\0') {
    sendJsonResponse_P(request, 400, PSTR("{\"status\":\"error\",\"msg\":\"invalid\"}"));
    return;
  }

  time_t epoch = (raw > 20000000000ULL) ? static_cast<time_t>(raw / 1000ULL) : static_cast<time_t>(raw);
  if (epoch <= NTP_VALID_TIMESTAMP_THRESHOLD) {
    sendJsonResponse_P(request, 400, PSTR("{\"status\":\"error\",\"msg\":\"too_old\"}"));
    return;
  }

  if (m_ntpClient.getTimeSource() == NtpClient::TimeSource::NTP) {
    sendJsonResponse_P(request, 200, PSTR("{\"status\":\"ignored\"}"));
    return;
  }

  m_ntpClient.setManualTime(epoch);
  sendJsonResponse_P(request, 200, PSTR("{\"status\":\"ok\"}"));
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
    sendJsonResponse_P(request, 503, PSTR("{\"error\":\"Low memory\"}"));
    return;
  }
  AsyncResponseStream* response = request->beginResponseStream("application/json", 384);
  IPAddress ip = WiFi.localIP();

  // Get sensor readings
  const auto& cfg = m_configManager.getConfig();
  const auto effective = SensorNormalization::makeEffectiveSensorSnapshot(m_sensorManager.getTemp(),
                                                                          m_sensorManager.getHumidity(),
                                                                          m_sensorManager.getLight(),
                                                                          cfg.TEMP_OFFSET,
                                                                          cfg.HUMIDITY_OFFSET,
                                                                          cfg.LUX_SCALING_FACTOR);

  // Escape SSID to prevent JSON injection (uses stack buffer)
  char ssidRaw[WIFI_SSID_MAX_LEN] = REDACTED
  WiFi.SSID().toCharArray(ssidRaw, sizeof(ssidRaw));
  char safeSsid[68];
  (void)Utils::escape_json_string(std::span{safeSsid}, ssidRaw);

  // CHANGED: Escape Firmware Version to prevent injection via build flags
  char safeFw[32];
  (void)Utils::escape_json_string(std::span{safeFw}, FIRMWARE_VERSION);

  auto& health = SystemHealth::HealthMonitor::instance();
  uint32_t minFreeHeap = health.getMinFreeHeap();
  uint32_t minMaxBlock = health.getMinMaxBlock();
  char tempValid[6];
  char humValid[6];
  char luxValid[6];
  copy_json_bool(tempValid, sizeof(tempValid), effective.temperature.isValid);
  copy_json_bool(humValid, sizeof(humValid), effective.humidity.isValid);
  copy_json_bool(luxValid, sizeof(luxValid), effective.light.isValid);

  response->printf_P(
      PSTR("{\"firmware\":\"%s\",\"nodeId\":\"%d-%d\",\"freeHeap\":%u,\"minFreeHeap\":%u,\"minMaxBlock\":%u,"
           "\"uptime\":\"%luh\",\"ssid\":\"%s\",\"ip\":\"%u.%u.%u.%u\",\"temperature\":%.1f,\"humidity\":%.1f,"
           "\"lux\":%u,\"tempValid\":%s,\"humValid\":%s,\"luxValid\":%s}"),
      safeFw,
      GH_ID,
      NODE_ID,
      ESP.getFreeHeap(),
      minFreeHeap,
      minMaxBlock,
      millis() / 3600000UL,
      safeSsid,
      ip[0],
      ip[1],
      ip[2],
      ip[3],
      effective.temperature.isValid ? effective.temperature.effectiveValue : 0.0f,
      effective.humidity.isValid ? effective.humidity.effectiveValue : 0.0f,
      effective.light.isValid ? static_cast<uint16_t>(effective.light.effectiveValue) : 0,
      tempValid,
      humValid,
      luxValid);
  request->send(response);
}

void AppServer::handleWifiSavedRequest(AsyncWebServerRequest* request) {
  if (ESP.getFreeHeap() < 2500) {
    sendJsonResponse_P(request, 503, PSTR("{\"error\":\"Low memory\"}"));
    return;
  }
  AsyncResponseStream* response = request->beginResponseStream("application/json", 768);
  auto& store = m_wifiManager.getCredentialStore();

  response->print(F("{"));

  // Primary GH
  response->print(F("\"primary\":"));
  auto* primary = store.getPrimaryGH();
  if (primary && !primary->isEmpty()) {
    char safePrimary[68];
    char available[6];
    (void)Utils::escape_json_string(std::span{safePrimary}, primary->ssid);
    copy_json_bool(available, sizeof(available), primary->isAvailable());
    response->printf_P(PSTR("{\"ssid\":REDACTED
                       safePrimary,
                       primary->lastRssi,
                       available);
  } else {
    response->print(F("null"));
  }

  // Secondary GH
  response->print(F(",\"secondary\":"));
  auto* secondary = store.getSecondaryGH();
  if (secondary && !secondary->isEmpty()) {
    char safeSecondary[68];
    char available[6];
    (void)Utils::escape_json_string(std::span{safeSecondary}, secondary->ssid);
    copy_json_bool(available, sizeof(available), secondary->isAvailable());
    response->printf_P(PSTR("{\"ssid\":REDACTED
                       safeSecondary,
                       secondary->lastRssi,
                       available);
  } else {
    response->print(F("null"));
  }

  // User saved credentials
  response->print(F(",\"saved\":["));
  auto saved = store.getSavedCredentials();
  bool first = true;
  for (auto& cred : saved) {
    if (!cred.isEmpty()) {
      if (!first)
        response->print(F(","));
      first = false;
      char safeSsid[68];
      char available[6];
      char hidden[6];
      (void)Utils::escape_json_string(std::span{safeSsid}, cred.ssid);
      copy_json_bool(available, sizeof(available), cred.isAvailable());
      copy_json_bool(hidden, sizeof(hidden), cred.isHidden());
      response->printf_P(PSTR("{\"ssid\":REDACTED
                         safeSsid,
                         cred.lastRssi,
                         available,
                         hidden);
    }
  }
  response->print(F("]}"));

  store.releaseSavedCredentials();
  request->send(response);
}

void AppServer::handleNetworksRequest(AsyncWebServerRequest* request) {
  if (ESP.getFreeHeap() < 4500 || ESP.getMaxFreeBlockSize() < 2500) {
    sendJsonResponse_P(request, 503, PSTR("{\"error\":\"Low memory\"}"));
    return;
  }

  const bool forceRefresh = request->hasParam("refresh");
  if (m_wifiManager.isScanBusy()) {
    sendJsonResponse_P(request, 200, PSTR("{\"scanning\":true}"));
    return;
  }

  WifiManager:REDACTED
  uint8_t count = m_wifiManager.copyScanResults(results, WifiManager::MAX_SCAN_RESULTS);
  const bool hasSnapshot = m_wifiManager.hasScanSnapshot();

  if (!forceRefresh && hasSnapshot) {
    AsyncResponseStream* response = request->beginResponseStream("application/json", 384);
    response->print(F("{\"networks\":["));

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

    response->print(F("]}"));
    store.releaseSavedCredentials();
    request->send(response);
    return;
  }

  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING || n >= 0) {
    sendJsonResponse_P(request, 200, PSTR("{\"scanning\":true}"));
    return;
  }

  unsigned long now = millis();
  if (now - m_lastScanRequest < 2000) {
    sendJsonResponse_P(request, 429, PSTR("{\"error\":\"Rate limited\"}"));
    return;
  }
  m_lastScanRequest = now;

  // Trigger non-blocking scan via WifiManager to avoid WDT in async context.
  switch (m_wifiManager.requestNetworkScan()) {
    case WifiManager:REDACTED
    case WifiManager:REDACTED
      sendJsonResponse_P(request, 200, PSTR("{\"scanning\":true}"));
      break;
    case WifiManager:REDACTED
      sendJsonResponse_P(request, 503, PSTR("{\"error\":\"Low memory\"}"));
      break;
    default:
      sendJsonResponse_P(request, 200, PSTR("{\"networks\":[]}"));
      break;
  }
}

void AppServer::handleSaveRequest(AsyncWebServerRequest* request) {
  if (ESP.getFreeHeap() < 6000) {
    sendJsonResponse_P(request, 503, PSTR("{\"status\":\"error\",\"message\":\"Low memory\"}"));
    return;
  }

  char ssid[33] = REDACTED
  char pass[65] = REDACTED
  bool hidden = request->hasArg("hidden");

  if (!request->hasArg("REDACTED")) {
    sendTextResponse_P(request, 400, PSTR("REDACTED"));
    return;
  }

  request->arg("REDACTED").toCharArray(ssid, sizeof(ssid));

  if (request->hasArg("REDACTED")) {
    request->arg("REDACTED").toCharArray(pass, sizeof(pass));
  }

  size_t ssidLen = REDACTED
  size_t passLen = REDACTED

  if (ssidLen =REDACTED
    sendTextResponse_P(request, 400, PSTR("Invalid Input"));
    return;
  }
  if (!Utils::isSafeString(std::string_view(ssid, ssidLen)) ||
      !Utils::isSafeString(std::string_view(pass, passLen))) {
    sendTextResponse_P(request, 400, PSTR("Invalid Characters"));
    return;
  }

  if (m_wifiManager.addUserCredential(std:REDACTED
                                      std::string_view(pass, passLen),
                                      hidden)) {
    sendJsonResponse_P(request, 200, PSTR("{\"status\":\"ok\"}"));
  } else {
    sendJsonResponse_P(request, 500, PSTR("{\"status\":\"error\",\"message\":\"Failed to save\"}"));
  }
}

void AppServer::handleForgetRequest(AsyncWebServerRequest* request) {
  if (ESP.getFreeHeap() < 6000) {
    sendJsonResponse_P(request, 503, PSTR("{\"status\":\"error\",\"message\":\"Low memory\"}"));
    return;
  }

  char ssid[33] = REDACTED
  if (request->hasArg("REDACTED")) {
    request->arg("REDACTED").toCharArray(ssid, sizeof(ssid));
  }
  size_t ssidLen = REDACTED
  if (ssidLen > 0 && m_wifiManager.removeUserCredential(std:REDACTED
    sendJsonResponse_P(request, 200, PSTR("{\"status\":\"ok\"}"));
  } else {
    sendJsonResponse_P(request, 400, PSTR("{\"status\":\"error\"}"));
  }
}

void AppServer::handleFormatFsRequest(AsyncWebServerRequest* request) {
  if (m_formatFsPending) {
    sendJsonResponse_P(request, 202, PSTR("{\"status\":\"pending\"}"));
    return;
  }

  LOG_WARN("APP", F("Filesystem format requested from dashboard. Scheduled."));
  m_formatFsPending = true;
  sendJsonResponse_P(request, 200, PSTR("{\"status\":\"ok\",\"message\":\"Filesystem format scheduled. Device will reboot.\"}"));
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
      sendTextResponse_P(request, 429, PSTR("Too Many Requests (System Locked)"));
      return false;
    }
    m_ota_fail_count = REDACTED
  }

  // Authentication check
  char hashed_pass_buffer[65];
  char passArg[MAX_PASS_LEN] = REDACTED
  if (request->hasParam("REDACTED", true)) {
    const String& passStr = REDACTED
    passStr.toCharArray(passArg, sizeof(passArg));
  }

  size_t passLen = REDACTED
  (void)Utils::hash_sha256(std::span<char>(hashed_pass_buffer), std::string_view(passArg, passLen));

  if (!Utils::consttime_equal(hashed_pass_buffer, m_configManager.getAdminPassword(), 64)) {
    LOG_WARN("REDACTED", F("REDACTED"));
    m_ota_fail_count++;
    m_ota_lockout_ts = REDACTED
    m_configManager.releaseStrings();
    sendTextResponse_P(request, 401, PSTR("REDACTED"));
    return false;
  }

  m_configManager.releaseStrings();
  m_ota_fail_count = REDACTED

  // Check filesystem space
  size_t content_len = request->contentLength();
  FSInfo fs_info;
  LittleFS.info(fs_info);
  if (fs_info.totalBytes - fs_info.usedBytes < content_len) {
    sendTextResponse_P(request, 413, PSTR("Not enough filesystem space"));
    return false;
  }

  // Enable Async mode to prevent Update.write() from yielding in SYS context
  Update.runAsync(true);

  if (!Update.begin(content_len, U_FLASH)) {
    LOG_ERROR("WEB-OTA", F("Update Start Failed: REDACTED
    sendTextResponse_P(request, 500, PSTR("Update Start Failed"));
    return false;
  }

  startWebOtaSession(content_len);
  if (m_otaStartCallback)
    m_otaStartCallback();
  return true;
}


bool AppServer::handleOtaWrite(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
  if (!Update.isRunning())
    return false;

  if (Update.write(data, len) != len) {
    LOG_ERROR("WEB-OTA", F("Write Failed: REDACTED
    sendTextResponse_P(request, 500, PSTR("Write Failed"));
    if (Update.isRunning()) {
      (void)Update.end();
    }
    if (m_otaEndCallback)
      m_otaEndCallback();  // Resume on failure
    finishWebOtaSession();
    return false;
  }
  touchWebOtaSession(len);
  return true;
}


void AppServer::handleOtaFinalize(AsyncWebServerRequest* request, size_t totalSize) {
  if (Update.end(true)) {
    LOG_INFO("WEB-OTA", F("Success. Total: REDACTED
    sendTextResponse_P(request, 200, PSTR("Success! Rebooting..."));
    m_rebootRequired = true;
    m_rebootTimestamp = millis();
    if (m_otaEndCallback)
      m_otaEndCallback();
    finishWebOtaSession();
  } else {
    LOG_ERROR("WEB-OTA", F("Update End Failed: REDACTED
    sendTextResponse_P(request, 500, PSTR("Update End Failed"));
    if (Update.isRunning()) {
      (void)Update.end();
    }
    if (m_otaEndCallback)
      m_otaEndCallback();
    finishWebOtaSession();
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
