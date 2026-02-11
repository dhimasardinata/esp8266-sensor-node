#include "PortalServer.h"

#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>

#include <string_view>

#include "CryptoUtils.h"
#include "Logger.h"
#include "REDACTED"
#include "WebAppData.h"
#include "utils.h"

// PortalServer.Routes.cpp - HTTP routes and JSON helpers

namespace {
  void sendConnectingPage(AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse_P(200, CONNECTING_HTML_MIME, CONNECTING_HTML, CONNECTING_HTML_LEN);
    if (CONNECTING_HTML_GZIPPED)
      response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  }
}  // namespace

String PortalServer::templateProcessor(const String& var) {
  if (var == "ERROR_MSG") {
    return (m_portalStatus == PortalStatus::FAIL) ? "Connection failed." : "";
  }
  if (var == "ERROR_DISPLAY") {
    return (m_portalStatus == PortalStatus::FAIL) ? "block" : "none";
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
      response->addHeader("Content-Encoding", "gzip");
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
        request->send(503, "text/plain", "Low memory");
        return;
      }
    }
    AsyncWebServerResponse* response =
        request->beginResponse_P(200, CRYPTO_JS_MIME, CRYPTO_JS, CRYPTO_JS_LEN);
    if (CRYPTO_JS_GZIPPED)
      response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  })));

  storeHandler(&(m_server.on("/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    this->handleSaveRequest(request);
  })));

  storeHandler(&(m_server.on("/connecting", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse_P(200, CONNECTING_HTML_MIME, CONNECTING_HTML, CONNECTING_HTML_LEN);
    if (CONNECTING_HTML_GZIPPED)
      response->addHeader("Content-Encoding", "gzip");
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
      response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  })));

  storeHandler(&(m_server.on("/rescan", HTTP_POST, [this](AsyncWebServerRequest* request) {
    m_reboot_scheduled = true;
    m_rebootTimer.reset();
    LOG_WARN("PORTAL", F("Rescan requested. Rebooting..."));
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  })));

  storeHandler(&(m_server.on("/factory-reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
    this->handleFactoryResetRequest(request);
  })));

  m_server.onNotFound([this, sendPortalRoot](AsyncWebServerRequest* request) {
    if (m_isRunning) {
      sendPortalRoot(request);
    } else {
      request->send(404);
    }
  });
}

void PortalServer::handleSaveRequest(AsyncWebServerRequest* request) {
  // Usage of stack buffers prevents heap fragmentation.
  char ssid[33] = REDACTED
  // Encrypted payload for a <=64 byte password is ~133 bytes max (base64 + iv).
  constexpr size_t MAX_PASS_PAYLOAD = REDACTED
  char pass[MAX_PASS_PAYLOAD] = REDACTED
  bool hidden = request->hasArg("hidden");

  if (!request->hasArg("REDACTED")) {
    request->send_P(400, "REDACTED", PSTR("REDACTED"));
    return;
  }

  request->arg("REDACTED").toCharArray(ssid, sizeof(ssid));

  if (request->hasArg("REDACTED")) {
    const String passArg = REDACTED
    if (passArg.length() >= REDACTED
      request->send(400, "REDACTED", "REDACTED");
      return;
    }
    passArg.toCharArray(pass, sizeof(pass));
  }

  size_t passLen = REDACTED

  // Decrypt if encrypted
  if (passLen > 4 && memcmp(pass, "ENC:REDACTED
    std::string_view encryptedPayloadView(pass + 4, passLen - 4);

    auto payload = CryptoUtils::deserialize_payload(encryptedPayloadView);
    if (payload) {
      const CryptoUtils::AES_CBC_Cipher* cipher = m_portalCipher;
      char decryptedPass[65];
      size_t decLen = 0;
      if (cipher->decrypt(*payload, decryptedPass, sizeof(decryptedPass) - 1, decLen)) {
        decryptedPass[decLen] = REDACTED
        size_t copyLen = std::min(decLen, sizeof(pass) - 1);
        memcpy(pass, decryptedPass, copyLen);
        pass[copyLen] = REDACTED
        LOG_INFO("REDACTED", "REDACTED", decLen);
        passLen = REDACTED
      } else {
        LOG_ERROR("PORTAL", "Decryption failed - Redirecting to error page");
        m_portalStatus = PortalStatus::DECRYPTION_FAIL;
        sendConnectingPage(request);
        return;
      }
    } else {
      LOG_ERROR("PORTAL", "Invalid payload format");
      request->send(400, "text/plain", "Invalid encryption payload.");
      return;
    }
  }

  size_t ssidLen = REDACTED
  passLen = REDACTED

  if (ssidLen =REDACTED
    request->send_P(400, "text/plain", PSTR("Invalid Input"));
    return;
  }
  if (!Utils::isSafeString(std::string_view(ssid, ssidLen)) ||
      !Utils::isSafeString(std::string_view(pass, passLen))) {
    request->send_P(400, "text/plain", PSTR("Invalid Characters"));
    return;
  }
  if (!ConfigManager::saveTempWifiCredentials(std::string_view(ssid, ssidLen),
                                              std::string_view(pass, passLen),
                                              hidden)) {
    request->send_P(500, "text/plain", PSTR("Save Failed"));
    return;
  }
  m_portalStatus = PortalStatus::TESTING;
  m_portalTestTimer.reset();
  m_pendingConnection = true;
  sendConnectingPage(request);
}

void PortalServer::handleScanRequest(AsyncWebServerRequest* request) {
  m_scanResultsCached = false;
  m_cachedNetworkCount = 0;
  constexpr uint32_t kPortalScanMinHeap = 7000;
  constexpr uint32_t kPortalScanMinBlock = 3500;
  if ((WiFi.getMode() & WIFI_STA) =REDACTED
    request->send(200, "application/json", "{\"error\":\"sta_disabled\"}");
    return;
  }
  if (WiFi.scanComplete() =REDACTED
    request->send(200, "application/json", "{\"scanning\":true}");
    return;
  }
  if (ESP.getFreeHeap() < kPortalScanMinHeap || ESP.getMaxFreeBlockSize() < kPortalScanMinBlock) {
    LOG_WARN("PORTAL",
             F("Scan skipped (low heap: %u, block %u)"),
             ESP.getFreeHeap(),
             ESP.getMaxFreeBlockSize());
    m_wifiManager.requestPortalScan();
    request->send(200, "application/json", "{\"scanning\":true,\"mode\":\"ap_off\"}");
    return;
  }
  WiFi.scanDelete();
  WiFi.scanNetworksAsync([](int result) { (void)result; }, false);
  m_portalScanInProgress = true;
  request->send(200, "application/json", "{\"scanning\":true}");
}

void PortalServer::handleForgetRequest(AsyncWebServerRequest* request) {
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

void PortalServer::handleFactoryResetRequest(AsyncWebServerRequest* request) {
  LOG_WARN("PORTAL", F("Factory Reset requested. Scheduled."));
  m_factoryResetPending = true;  // Defer execution to main loop
  request->send(200, "text/plain", "Resetting...");
}


void PortalServer::sendStatusJson(AsyncWebServerRequest* request) const {
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  const char* statusStr = "idle";
  const char* msgStr = "";
  const char* detailStr = "";

  switch (m_portalStatus) {
    case PortalStatus::TESTING:
      statusStr = "testing";
      msgStr = "Connecting...";
      detailStr = "Attempting to connect to the network";
      break;
    case PortalStatus::SUCCESS:
      statusStr = "success";
      msgStr = "Success! Rebooting.";
      detailStr = "Connection successful, device will restart";
      break;
    case PortalStatus::FAIL:
      statusStr = "fail";
      msgStr = "Connection failed.";
      detailStr = "Wrong password or network not found";
      break;
    case PortalStatus::DECRYPTION_FAIL:
      statusStr = "fail";
      msgStr = "Security Error";
      detailStr = "Decryption failed. Check device time or key.";
      break;
    default:
      break;
  }

  response->printf("{\"status\":\"%s\",\"message\":\"%s\",\"detail\":\"%s\"}", statusStr, msgStr, detailStr);
  request->send(response);
}


void PortalServer::cacheNetworkScanResults() {
  int n = WiFi.scanComplete();
  if (n <= 0) {
    m_cachedNetworkCount = 0;
    m_portalScanInProgress = false;
    return;
  }

  if (!ensureCacheBuffer()) {
    m_cachedNetworkCount = 0;
    m_portalScanInProgress = false;
    WiFi.scanDelete();
    return;
  }

  m_cachedNetworkCount = 0;
  auto& store = m_wifiManager.getCredentialStore();
  String ssidTmp;
  ssidTmp.reserve(WIFI_SSID_MAX_LEN);
  for (int i = 0; i < n && m_cachedNetworkCount < MAX_CACHED_NETWORKS; i++) {
    ssidTmp = REDACTED
    if (ssidTmp.length() =REDACTED
      continue;

    CachedNetwork& net = m_cachedNetworks[m_cachedNetworkCount];
    const char* ssid = REDACTED
    if (!ssid || ssid[0] =REDACTED
      continue;
    size_t ssidLen = REDACTED
    if (ssidLen >= REDACTED
      ssidLen = REDACTED
    memcpy(net.ssid, ssid, ssidLen);
    net.ssid[ssidLen] = REDACTED
    net.rssi = WiFi.RSSI(i);
    net.bars = WifiRouteUtils::computeSignalBars(net.rssi);
    net.isOpen = (WiFi.encryptionType(i) == ENC_TYPE_NONE);
    net.isKnown = store.hasCredential(ssid);
    m_cachedNetworkCount++;
  }

  store.releaseSavedCredentials();

  m_lastScanTime = millis();
  m_scanResultsCached = true;
  m_portalScanInProgress = false;
  WiFi.scanDelete();
}


void PortalServer::sendNetworksJson(AsyncWebServerRequest* request) {
  uint32_t now = millis();
  constexpr uint32_t kPortalScanThrottleMs = 5000;
  constexpr uint32_t kPortalScanMinHeap = 7000;
  constexpr uint32_t kPortalScanMinBlock = 3500;

  auto sendCachedNetworks = [&]() {
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->print("{\"networks\":[");

    bool first = true;
    if (m_cachedNetworks) {
      for (uint8_t i = 0; i < m_cachedNetworkCount; i++) {
        const CachedNetwork& net = m_cachedNetworks[i];
        (void)WifiRouteUtils:REDACTED
            *response, first, net.ssid, net.rssi, net.isOpen, net.isKnown);
      }
    }

    response->print("]}");
    request->send(response);
  };

  if ((WiFi.getMode() & WIFI_STA) =REDACTED
    if (m_scanResultsCached) {
      sendCachedNetworks();
    } else {
      request->send(200, "application/json", "{\"error\":\"sta_disabled\"}");
    }
    return;
  }

  if (m_scanResultsCached && (now - m_lastScanTime) < SCAN_CACHE_DURATION_MS) {
    sendCachedNetworks();
    return;
  }

  if (m_scanResultsCached &&
      (ESP.getFreeHeap() < kPortalScanMinHeap ||
       ESP.getMaxFreeBlockSize() < kPortalScanMinBlock)) {
    sendCachedNetworks();
    return;
  }

  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    m_portalScanInProgress = true;
    request->send(200, "application/json", "{\"scanning\":true}");
    return;
  }

  if (n > 0) {
    cacheNetworkScanResults();
    sendNetworksJson(request);
    return;
  }

  m_portalScanInProgress = false;
  // Throttle and avoid scans under low heap to prevent OOM in scan internals.
  if ((now - m_lastScanStart) < kPortalScanThrottleMs ||
      ESP.getFreeHeap() < kPortalScanMinHeap ||
      ESP.getMaxFreeBlockSize() < kPortalScanMinBlock) {
    if (ESP.getFreeHeap() < kPortalScanMinHeap || ESP.getMaxFreeBlockSize() < kPortalScanMinBlock) {
      request->send(200, "application/json", "{\"error\":\"low_memory\"}");
    } else {
      request->send(200, "application/json", "{\"scanning\":true}");
    }
    return;
  }
  m_lastScanStart = now;
  WiFi.scanDelete();
  WiFi.scanNetworksAsync([](int result) { (void)result; }, false);
  m_portalScanInProgress = true;
  request->send(200, "application/json", "{\"scanning\":true}");
}


void PortalServer::sendSavedCredentialsJson(AsyncWebServerRequest* request) const {
  auto& store = m_wifiManager.getCredentialStore();

  AsyncResponseStream* response = request->beginResponseStream("application/json", 512);
  response->print("{\"credentials\":[");
  bool first = true;

  const auto* primary = store.getPrimaryGH();
  const auto* secondary = store.getSecondaryGH();

  auto appendCred = [&](const char* ssid, bool builtin, bool available) {
    char safeSsid[68];
    (void)Utils::escape_json_string(std::span{safeSsid}, ssid);
    if (!first)
      response->print(",");
    first = false;
    response->printf("{\"ssid\":REDACTED
                     safeSsid,
                     builtin ? "true" : "false",
                     available ? "true" : "false");
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

  response->print("]}");
  store.releaseSavedCredentials();
  request->send(response);
}
