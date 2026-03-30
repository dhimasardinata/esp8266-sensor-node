#include "web/PortalServer.h"

#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>

#include <string_view>

#include "support/CryptoUtils.h"
#include "system/Logger.h"
#include "net/NtpClient.h"
#include "REDACTED"
#include "generated/WebAppData.h"
#include "support/Utils.h"

// PortalServer.Routes.cpp - HTTP routes and JSON helpers

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

  void sendConnectingPage(AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse_P(200, CONNECTING_HTML_MIME, CONNECTING_HTML, CONNECTING_HTML_LEN);
    if (CONNECTING_HTML_GZIPPED)
      response->addHeader(F("Content-Encoding"), F("gzip"));
    request->send(response);
  }
}  // namespace

String PortalServer::templateProcessor(const String& var) {
  if (var == "ERROR_MSG") {
    return (m_portalStatus == PortalStatus::FAIL) ? String(F("Connection failed.")) : String();
  }
  if (var == "ERROR_DISPLAY") {
    return (m_portalStatus == PortalStatus::FAIL) ? String(F("block")) : String(F("none"));
  }
  if (var == "HOST_NAME") {
    char h[32];
    m_configManager.getHostname(h, sizeof(h));
    return String(h);  // String wrapper here is unfortunately required by the processor interface
  }
  return String();
}


void PortalServer::setupRoutes() {
  if (m_routesInitialized)
    return;
  m_routesInitialized = true;
  m_handlerCount = 0;
  constexpr size_t kMaxHandlers = sizeof(m_handlers) / sizeof(m_handlers[0]);
  auto storeHandler = [&](AsyncWebHandler* handler) {
    if (!handler)
      return;
    handler->setFilter([this](AsyncWebServerRequest* request) {
      (void)request;
      return m_isRunning;
    });
    if (m_handlerCount < kMaxHandlers) {
      m_handlers[m_handlerCount++] = handler;
    } else {
      LOG_ERROR("PORTAL", F("Handler table full; route skipped"));
    }
  };

  auto sendPortalRoot = [this](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse_P(200, PORTAL_HTML_MIME, PORTAL_HTML, PORTAL_HTML_LEN);
    if (PORTAL_HTML_GZIPPED)
      response->addHeader(F("Content-Encoding"), F("gzip"));
    request->send(response);
  };

  storeHandler(&(m_server.on("/", HTTP_GET, [sendPortalRoot](AsyncWebServerRequest* request) {
    sendPortalRoot(request);
  })));

  // Route for crypto.js
  storeHandler(&(m_server.on("/crypto.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (CRYPTO_JS_GZIPPED) {
      constexpr uint32_t kMinHeapForHeader = 2048;
      constexpr uint32_t kMinBlockForHeader = 1024;
      if (ESP.getFreeHeap() < kMinHeapForHeader || ESP.getMaxFreeBlockSize() < kMinBlockForHeader) {
        sendTextResponse_P(request, 503, PSTR("Low memory"));
        return;
      }
    }
    AsyncWebServerResponse* response =
        request->beginResponse_P(200, CRYPTO_JS_MIME, CRYPTO_JS, CRYPTO_JS_LEN);
    if (CRYPTO_JS_GZIPPED)
      response->addHeader(F("Content-Encoding"), F("gzip"));
    request->send(response);
  })));

  storeHandler(&(m_server.on("/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    this->handleSaveRequest(request);
  })));

  storeHandler(&(m_server.on("/connecting", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse_P(200, CONNECTING_HTML_MIME, CONNECTING_HTML, CONNECTING_HTML_LEN);
    if (CONNECTING_HTML_GZIPPED)
      response->addHeader(F("Content-Encoding"), F("gzip"));
    request->send(response);
  })));

  storeHandler(
      &(m_server.on("/status", HTTP_GET, [this](AsyncWebServerRequest* request) { this->sendStatusJson(request); })));

  storeHandler(&(
      m_server.on("/networks", HTTP_GET, [this](AsyncWebServerRequest* request) { this->sendNetworksJson(request); })));

  storeHandler(&(m_server.on("/scan", HTTP_POST, [this](AsyncWebServerRequest* request) {
    this->handleScanRequest(request);
  })));

  storeHandler(&(m_server.on("/saved", HTTP_GET, [this](AsyncWebServerRequest* request) {
    this->sendSavedCredentialsJson(request);
  })));

  storeHandler(&(m_server.on("/forget", HTTP_POST, [this](AsyncWebServerRequest* request) {
    this->handleForgetRequest(request);
  })));

  storeHandler(&(m_server.on("/success", HTTP_GET, [this](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse_P(200, REBOOTING_HTML_MIME, REBOOTING_HTML, REBOOTING_HTML_LEN);
    if (REBOOTING_HTML_GZIPPED)
      response->addHeader(F("Content-Encoding"), F("gzip"));
    request->send(response);
  })));

  storeHandler(&(m_server.on("/rescan", HTTP_POST, [this](AsyncWebServerRequest* request) {
    m_reboot_scheduled = true;
    m_rebootTimer.reset();
    LOG_WARN("PORTAL", F("Rescan requested. Rebooting..."));
    sendJsonResponse_P(request, 200, PSTR("{\"status\":\"ok\"}"));
  })));

  storeHandler(&(m_server.on("/factory-reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
    this->handleFactoryResetRequest(request);
  })));

  storeHandler(&(m_server.on("/time", HTTP_POST, [this](AsyncWebServerRequest* request) {
    this->handleTimeRequest(request);
  })));
  storeHandler(&(m_server.on("/time", HTTP_GET, [this](AsyncWebServerRequest* request) {
    this->handleTimeRequest(request);
  })));

  m_server.onNotFound([this, sendPortalRoot](AsyncWebServerRequest* request) {
    if (m_isRunning) {
      sendPortalRoot(request);
    } else {
      request->send(404);
    }
  });
}

void PortalServer::handleTimeRequest(AsyncWebServerRequest* request) {
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

  // Accept milliseconds and convert to seconds if needed.
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

void PortalServer::handleSaveRequest(AsyncWebServerRequest* request) {
  // Usage of stack buffers prevents heap fragmentation.
  char ssid[33] = REDACTED
  // Encrypted payload for a <=64 byte password is ~133 bytes max (base64 + iv).
  constexpr size_t MAX_PASS_PAYLOAD = REDACTED
  char pass[MAX_PASS_PAYLOAD] = REDACTED
  bool hidden = request->hasArg("hidden");

  if (!request->hasArg("REDACTED")) {
    sendTextResponse_P(request, 400, PSTR("REDACTED"));
    return;
  }

  request->arg("REDACTED").toCharArray(ssid, sizeof(ssid));

  if (request->hasArg("REDACTED")) {
    const String& passArg = REDACTED
    if (passArg.length() >= REDACTED
      sendTextResponse_P(request, 400, PSTR("REDACTED"));
      return;
    }
    passArg.toCharArray(pass, sizeof(pass));
  }

  size_t passLen = REDACTED

  // Decrypt if encrypted
  if (passLen > 4 && memcmp_P(pass, PSTR("ENC:REDACTED
    std::string_view encryptedPayloadView(pass + 4, passLen - 4);

    auto payload = CryptoUtils::deserialize_payload(encryptedPayloadView);
    if (payload) {
      const auto& cipher = CryptoUtils::sharedCipher();
      char decryptedPass[65];
      size_t decLen = 0;
      if (cipher.decrypt(*payload, decryptedPass, sizeof(decryptedPass) - 1, decLen)) {
        decryptedPass[decLen] = REDACTED
        size_t copyLen = std::min(decLen, sizeof(pass) - 1);
        memcpy(pass, decryptedPass, copyLen);
        pass[copyLen] = REDACTED
        LOG_INFO("REDACTED", F("REDACTED"), decLen);
        passLen = REDACTED
      } else {
        LOG_ERROR("PORTAL", F("Decryption failed - Redirecting to error page"));
        m_portalStatus = PortalStatus::DECRYPTION_FAIL;
        sendConnectingPage(request);
        return;
      }
    } else {
      LOG_ERROR("PORTAL", F("Invalid payload format"));
      sendTextResponse_P(request, 400, PSTR("Invalid encryption payload."));
      return;
    }
  }

  size_t ssidLen = REDACTED
  passLen = REDACTED

  if (ssidLen =REDACTED
    sendTextResponse_P(request, 400, PSTR("Invalid Input"));
    return;
  }
  if (!Utils::isSafeString(std::string_view(ssid, ssidLen)) ||
      !Utils::isSafeString(std::string_view(pass, passLen))) {
    sendTextResponse_P(request, 400, PSTR("Invalid Characters"));
    return;
  }
  if (!ConfigManager::saveTempWifiCredentials(std::string_view(ssid, ssidLen),
                                              std::string_view(pass, passLen),
                                              hidden)) {
    sendTextResponse_P(request, 500, PSTR("Save Failed"));
    return;
  }
  m_portalStatus = PortalStatus::TESTING;
  m_portalTestTimer.reset();
  m_pendingConnection = true;
  sendConnectingPage(request);
}

void PortalServer::handleScanRequest(AsyncWebServerRequest* request) {
  m_scanResultsCached = false;
  constexpr uint32_t kPortalScanMinHeap = 7000;
  constexpr uint32_t kPortalScanMinBlock = 3500;
  if ((WiFi.getMode() & WIFI_STA) =REDACTED
    sendJsonResponse_P(request, 200, PSTR("{\"error\":\"sta_disabled\"}"));
    return;
  }
  if (WiFi.scanComplete() =REDACTED
    sendJsonResponse_P(request, 200, PSTR("{\"scanning\":true}"));
    return;
  }
  if (ESP.getFreeHeap() < kPortalScanMinHeap || ESP.getMaxFreeBlockSize() < kPortalScanMinBlock) {
    LOG_WARN("PORTAL",
             F("Scan skipped (low heap: %u, block %u)"),
             ESP.getFreeHeap(),
             ESP.getMaxFreeBlockSize());
    m_wifiManager.requestPortalScan();
    sendJsonResponse_P(request, 200, PSTR("{\"scanning\":true,\"mode\":\"ap_off\"}"));
    return;
  }
  WiFi.scanDelete();
  WiFi.scanNetworksAsync([](int result) { (void)result; }, false);
  m_portalScanInProgress = true;
  sendJsonResponse_P(request, 200, PSTR("{\"scanning\":true}"));
}

void PortalServer::handleForgetRequest(AsyncWebServerRequest* request) {
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

void PortalServer::handleFactoryResetRequest(AsyncWebServerRequest* request) {
  LOG_WARN("PORTAL", F("Factory Reset requested. Scheduled."));
  m_factoryResetPending = true;  // Defer execution to main loop
  sendTextResponse_P(request, 200, PSTR("Resetting..."));
}


void PortalServer::sendStatusJson(AsyncWebServerRequest* request) const {
  AsyncResponseStream* response = request->beginResponseStream("application/json", 192);
  PGM_P statusP = PSTR("idle");
  PGM_P msgP = PSTR("");
  PGM_P detailP = PSTR("");

  switch (m_portalStatus) {
    case PortalStatus::TESTING:
      statusP = PSTR("testing");
      msgP = PSTR("Connecting...");
      detailP = PSTR("Attempting to connect to the network");
      break;
    case PortalStatus::SUCCESS:
      statusP = PSTR("success");
      msgP = PSTR("Success! Rebooting.");
      detailP = PSTR("Connection successful, device will restart");
      break;
    case PortalStatus::FAIL:
      statusP = PSTR("fail");
      msgP = PSTR("Connection failed.");
      detailP = PSTR("Wrong password or network not found");
      break;
    case PortalStatus::DECRYPTION_FAIL:
      statusP = PSTR("fail");
      msgP = PSTR("Security Error");
      detailP = PSTR("Decryption failed. Check device time or key.");
      break;
    default:
      break;
  }

  char statusStr[10];
  char msgStr[48];
  char detailStr[56];
  copy_trunc_P(statusStr, sizeof(statusStr), statusP);
  copy_trunc_P(msgStr, sizeof(msgStr), msgP);
  copy_trunc_P(detailStr, sizeof(detailStr), detailP);

  response->printf_P(PSTR("{\"status\":\"%s\",\"message\":\"%s\",\"detail\":\"%s\"}"),
                     statusStr,
                     msgStr,
                     detailStr);
  request->send(response);
}


void PortalServer::cacheNetworkScanResults() {
  int n = WiFi.scanComplete();
  if (n < 0) {
    m_scanResultsCached = false;
    m_portalScanInProgress = false;
    return;
  }

  (void)m_wifiManager.captureScanSnapshotFromWifi(n);
  m_lastScanTime = millis();
  m_scanResultsCached = m_wifiManager.hasScanSnapshot();
  m_portalScanInProgress = false;
  WiFi.scanDelete();
}


void PortalServer::sendNetworksJson(AsyncWebServerRequest* request) {
  uint32_t now = millis();
  constexpr uint32_t kPortalScanThrottleMs = 5000;
  constexpr uint32_t kPortalScanMinHeap = 7000;
  constexpr uint32_t kPortalScanMinBlock = 3500;

  auto sendCachedNetworks = [&]() -> bool {
    if (!m_wifiManager.hasScanSnapshot()) {
      return false;
    }
    WifiManager:REDACTED
    const uint8_t count = m_wifiManager.copyScanResults(results, WifiManager::MAX_SCAN_RESULTS);
    AsyncResponseStream* response = request->beginResponseStream("application/json", 384);
    response->print(F("{\"networks\":["));

    bool first = true;
    auto& store = m_wifiManager.getCredentialStore();
    for (uint8_t i = 0; i < count; ++i) {
      const char* ssid = REDACTED
      if (!ssid || ssid[0] =REDACTED
        continue;
      }
      const bool isKnown = store.hasCredential(ssid);
      (void)WifiRouteUtils:REDACTED
    }
    store.releaseSavedCredentials();

    response->print(F("]}"));
    request->send(response);
    return true;
  };

  if ((WiFi.getMode() & WIFI_STA) =REDACTED
    if ((m_scanResultsCached || refreshCachedNetworksFromWifiManager()) && sendCachedNetworks()) {
      return;
    } else {
      sendJsonResponse_P(request, 200, PSTR("{\"error\":\"sta_disabled\"}"));
    }
    return;
  }

  if (m_scanResultsCached &&
      (now - m_lastScanTime) < SCAN_CACHE_DURATION_MS &&
      sendCachedNetworks()) {
    return;
  }

  if (m_scanResultsCached &&
      (ESP.getFreeHeap() < kPortalScanMinHeap ||
       ESP.getMaxFreeBlockSize() < kPortalScanMinBlock) &&
      sendCachedNetworks()) {
    return;
  }

  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    m_portalScanInProgress = true;
    sendJsonResponse_P(request, 200, PSTR("{\"scanning\":true}"));
    return;
  }

  if (n >= 0) {
    cacheNetworkScanResults();
    if (sendCachedNetworks()) {
      return;
    }
    sendJsonResponse_P(request, 200, PSTR("{\"error\":\"low_memory\"}"));
    return;
  }

  if (refreshCachedNetworksFromWifiManager() && sendCachedNetworks()) {
    return;
  }

  m_portalScanInProgress = false;
  // Throttle and avoid scans under low heap to prevent OOM in scan internals.
  if ((now - m_lastScanStart) < kPortalScanThrottleMs ||
      ESP.getFreeHeap() < kPortalScanMinHeap ||
      ESP.getMaxFreeBlockSize() < kPortalScanMinBlock) {
    if (ESP.getFreeHeap() < kPortalScanMinHeap || ESP.getMaxFreeBlockSize() < kPortalScanMinBlock) {
      sendJsonResponse_P(request, 200, PSTR("{\"error\":\"low_memory\"}"));
    } else {
      sendJsonResponse_P(request, 200, PSTR("{\"scanning\":true}"));
    }
    return;
  }
  m_lastScanStart = now;
  WiFi.scanDelete();
  WiFi.scanNetworksAsync([](int result) { (void)result; }, false);
  m_portalScanInProgress = true;
  sendJsonResponse_P(request, 200, PSTR("{\"scanning\":true}"));
}


void PortalServer::sendSavedCredentialsJson(AsyncWebServerRequest* request) const {
  auto& store = m_wifiManager.getCredentialStore();

  AsyncResponseStream* response = request->beginResponseStream("application/json", 384);
  response->print(F("{\"credentials\":["));
  bool first = true;

  const auto* primary = store.getPrimaryGH();
  const auto* secondary = store.getSecondaryGH();

  auto appendCred = [&](const char* ssid, bool builtin, bool available) {
    char safeSsid[68];
    char builtinText[6];
    char availableText[6];
    (void)Utils::escape_json_string(std::span{safeSsid}, ssid);
    copy_json_bool(builtinText, sizeof(builtinText), builtin);
    copy_json_bool(availableText, sizeof(availableText), available);
    if (!first)
      response->print(F(","));
    first = false;
    response->printf_P(PSTR("{\"ssid\":REDACTED
                       safeSsid,
                       builtinText,
                       availableText);
  };

  if (primary && !primary->isEmpty()) {
    appendCred(primary->ssid, true, primary->isAvailable());
  }

  if (secondary && !secondary->isEmpty()) {
    appendCred(secondary->ssid, true, secondary->isAvailable());
  }

  for (const auto& cred : store.getSavedCredentials()) {
    if (!cred.isEmpty()) {
      appendCred(cred.ssid, false, cred.isAvailable());
    }
  }

  response->print(F("]}"));
  store.releaseSavedCredentials();
  request->send(response);
}
