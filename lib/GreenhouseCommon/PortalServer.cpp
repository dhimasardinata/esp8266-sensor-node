#include "PortalServer.h"
#include <ESPAsyncWebServer.h>

#include <ESP8266WiFi.h>

#include "WebAppData.h"
#include "utils.h"
#include "Logger.h"

PortalServer::PortalServer(AsyncWebServer& server, WifiManager& wifiManager, ConfigManager& configManager)
    : m_server(server),
      m_wifiManager(wifiManager),
      m_configManager(configManager),
      m_portalTestTimer(20000),
      m_rebootTimer(3000) {}

void PortalServer::onWifiStateChanged(WifiManager::State newState) {
  if (newState == WifiManager::State::PORTAL_MODE) {
    if (!m_isRunning)
      begin();
  } else {
    if (m_isRunning)
      stop();
  }
}

void PortalServer::begin() {
  if (m_isRunning)
    return;
  m_portalStatus = PortalStatus::IDLE;
  m_reboot_scheduled = false;
  m_dnsServer.start(53, "*", WiFi.softAPIP());
  setupRoutes();
  m_isRunning = true;
  LOG_INFO("PORTAL", F("Portal Server started."));
}

void PortalServer::stop() {
  if (!m_isRunning)
    return;
  m_dnsServer.stop();
  for (size_t i = 0; i < m_handlerCount; ++i) {
    m_server.removeHandler(m_handlers[i]);
  }
  m_handlerCount = 0;
  m_isRunning = false;
  LOG_INFO("PORTAL", F("Portal Server stopped."));
}

// =============================================================================
// handle() Helper Methods
// =============================================================================

void PortalServer::handlePendingConnection() {
  m_pendingConnection = false;
  LOG_INFO("WIFI", F("Applying new credentials..."));
  WiFi.mode(WIFI_STA);
  char s[64], p[64];
  bool h;
  ConfigManager::loadTempWifiCredentials(std::span{s}, std::span{p}, h);
  WiFi.begin(s, p);
}

void PortalServer::handleTestResult() {
  if (WiFi.status() == WL_CONNECTED) {
    m_portalStatus = PortalStatus::SUCCESS;
    
    // Save to permanent storage
    char s[64], p[64];
    bool h;
    ConfigManager::loadTempWifiCredentials(std::span{s}, std::span{p}, h);
    
    if (m_wifiManager.addUserCredential(s, p, h)) {
      LOG_INFO("PORTAL", F("Saved credentials for '%s' (hidden=%d)"), s, h);
    } else {
      LOG_ERROR("PORTAL", F("Failed to save credentials (full?)"));
    }
    
    ConfigManager::clearTempWifiCredentials(); 
    m_reboot_scheduled = true; 
    m_rebootTimer.reset();
  } else if (m_portalTestTimer.hasElapsed(false)) {
    m_portalStatus = PortalStatus::FAIL;
    WiFi.disconnect(true);
    ConfigManager::clearTempWifiCredentials();
  }
}

int PortalServer::computeSignalBars(int32_t rssi) {
  if (rssi > -50) return 4;
  if (rssi > -60) return 3;
  if (rssi > -70) return 2;
  if (rssi > -80) return 1;
  return 0;
}

// =============================================================================
// Main handle() - Refactored
// =============================================================================

void PortalServer::handle() {
  if (!m_isRunning) return;
  m_dnsServer.processNextRequest();

  if (m_pendingConnection) handlePendingConnection();
  if (m_portalStatus == PortalStatus::TESTING) handleTestResult();
  if (m_reboot_scheduled && m_rebootTimer.hasElapsed()) ESP.restart();
}

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
    return String(h); // String wrapper here is unfortunately required by the processor interface
  }
  return String();
}

void PortalServer::setupRoutes() {
  m_handlerCount = 0;
  m_handlers[m_handlerCount++] = &(m_server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, PORTAL_HTML_MIME, PORTAL_HTML, PORTAL_HTML_LEN, [this](const String& var) {
        return this->templateProcessor(var);
      }
    );
    if (PORTAL_HTML_GZIPPED) response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  }));

  m_handlers[m_handlerCount++] = &(m_server.on("/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    String ssid = request->arg("ssid");
    String pass = request->arg("pass");
    bool hidden = request->hasArg("hidden");
    
    if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64) {
        request->send_P(400, "text/plain", PSTR("Invalid Input"));
        return;
    }
    if (!Utils::isSafeString(ssid.c_str()) || !Utils::isSafeString(pass.c_str())) {
       request->send_P(400, "text/plain", PSTR("Invalid Characters"));
       return;
    }
    if (!ConfigManager::saveTempWifiCredentials(ssid.c_str(), pass.c_str(), hidden)) {
      request->send_P(500, "text/plain", PSTR("Save Failed"));
      return;
    }
    m_portalStatus = PortalStatus::TESTING;
    m_portalTestTimer.reset();
    m_pendingConnection = true;
    request->redirect("/connecting");
  }));

  m_handlers[m_handlerCount++] = &(m_server.on("/connecting", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, CONNECTING_HTML_MIME, CONNECTING_HTML, CONNECTING_HTML_LEN);
    if(CONNECTING_HTML_GZIPPED) response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  }));

  m_handlers[m_handlerCount++] = &(m_server.on("/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    this->sendStatusJson(request);
  }));

  m_handlers[m_handlerCount++] = &(m_server.on("/networks", HTTP_GET, [this](AsyncWebServerRequest* request) {
    this->sendNetworksJson(request);
  }));

  m_handlers[m_handlerCount++] = &(m_server.on("/saved", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "application/json", this->getSavedCredentialsJson());
  }));

  m_handlers[m_handlerCount++] = &(m_server.on("/forget", HTTP_POST, [this](AsyncWebServerRequest* request) {
    String ssid = request->arg("ssid");
    if (ssid.length() > 0 && m_wifiManager.removeUserCredential(ssid.c_str())) {
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        request->send(400, "application/json", "{\"status\":\"error\"}");
    }
  }));

  m_handlers[m_handlerCount++] = &(m_server.on("/success", HTTP_GET, [this](AsyncWebServerRequest* request) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, REBOOTING_HTML_MIME, REBOOTING_HTML, REBOOTING_HTML_LEN);
      if (REBOOTING_HTML_GZIPPED) response->addHeader("Content-Encoding", "gzip");
      request->send(response);
  }));

  m_server.onNotFound([this](AsyncWebServerRequest* request) { request->redirect("/"); });
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
    default:
      break;
  }

  response->printf("{\"status\":\"%s\",\"message\":\"%s\",\"detail\":\"%s\"}", statusStr, msgStr, detailStr);
  request->send(response);
}

void PortalServer::sendNetworksJson(AsyncWebServerRequest* request) const {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    request->send(200, "application/json", "{\"scanning\":true}");
    return;
  }

  if (n < 0) {
    WiFi.scanNetworks(true);
    request->send(200, "application/json", "{\"scanning\":true}");
    return;
  }

  AsyncResponseStream* response = request->beginResponseStream("application/json");
  response->print("{\"networks\":[");
  
  bool first = true;
  for (int i = 0; i < n && i < 15; i++) {
    if (!first) response->print(",");
    first = false;

    String ssid = WiFi.SSID(i);
    int32_t rssi = WiFi.RSSI(i);
    bool isOpen = (WiFi.encryptionType(i) == ENC_TYPE_NONE);
    int bars = computeSignalBars(rssi);

    bool isKnown = m_wifiManager.getCredentialStore().hasCredential(ssid.c_str());

    response->printf("{\"ssid\":\"%s\",\"rssi\":%d,\"bars\":%d,\"open\":%s,\"known\":%s}", 
      ssid.c_str(), rssi, bars, isOpen ? "true" : "false", isKnown ? "true" : "false");
  }

  response->print("]}");
  request->send(response);
}

String PortalServer::getSavedCredentialsJson() const {
  auto& store = m_wifiManager.getCredentialStore();
  
  // Use a string (unfortunately required by the request->send interface) 
  // but build it more efficiently
  String json;
  json.reserve(256);
  json = "{\"credentials\":[";
  bool first = true;
  
  const auto* primary = store.getPrimaryGH();
  const auto* secondary = store.getSecondaryGH();
  
  auto appendCred = [&](const char* ssid, bool builtin, bool available) {
    if (!first) json += ",";
    first = false;
    json += "{\"ssid\":\"";
    json += ssid;
    json += "\",\"builtin\":";
    json += builtin ? "true" : "false";
    json += ",\"available\":";
    json += available ? "true" : "false";
    json += "}";
  };

  if (primary && !primary->isEmpty()) {
    appendCred(primary->ssid, true, primary->isAvailable);
  }
  
  if (secondary && !secondary->isEmpty()) {
    appendCred(secondary->ssid, true, secondary->isAvailable);
  }
  
  for (const auto& cred : store.getSavedCredentials()) {
    if (!cred.isEmpty()) {
      appendCred(cred.ssid, false, cred.isAvailable);
    }
  }
  
  json += "]}";
  return json;
}