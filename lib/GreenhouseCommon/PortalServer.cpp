#include "PortalServer.h"

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
  stop();
  m_portalStatus = PortalStatus::IDLE;
  m_reboot_scheduled = false;
  m_dnsServer.start(53, "*", WiFi.softAPIP());
  setupRoutes();
  m_server.begin();
  m_isRunning = true;
}

void PortalServer::stop() {
  if (!m_isRunning)
    return;
  m_dnsServer.stop();
  m_server.reset();
  m_isRunning = false;
}

void PortalServer::handle() {
  if (!m_isRunning)
    return;
  m_dnsServer.processNextRequest();
  if (m_portalStatus == PortalStatus::TESTING) {
    if (WiFi.status() == WL_CONNECTED) {
      m_portalStatus = PortalStatus::SUCCESS;
      if (ConfigManager::promoteTempWifiCredentials()) {
        m_reboot_scheduled = true;
        m_rebootTimer.reset();
      }
    } else if (m_portalTestTimer.hasElapsed(false)) {
      m_portalStatus = PortalStatus::FAIL;
      WiFi.disconnect(true);
      ConfigManager::clearTempWifiCredentials();
    }
  }
  if (m_reboot_scheduled && m_rebootTimer.hasElapsed()) {
    ESP.restart();
  }
}

String PortalServer::templateProcessor(const String& var) {
  if (var == "ERROR_MSG") {
    return (m_portalStatus == PortalStatus::FAIL) ? "Connection failed." : "";
  }
  if (var == "ERROR_DISPLAY") {
    return (m_portalStatus == PortalStatus::FAIL) ? "block" : "none";
  }
  if (var == "HOST_NAME") {
    return m_configManager.getHostname();
  }
  return String();
}

void PortalServer::setupRoutes() {
  m_server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    // 1. Buat Objek Response dari Data PROGMEM
    AsyncWebServerResponse *response = request->beginResponse_P(200, PORTAL_HTML_MIME, PORTAL_HTML, PORTAL_HTML_LEN, [this](const String& var) {
        return this->templateProcessor(var);
      }
    );
    
    // 2. Cek apakah data dikompresi oleh script Python?
    // Variabel PORTAL_HTML_GZIPPED dibuat otomatis oleh web_to_header.py
    if (PORTAL_HTML_GZIPPED) {
      response->addHeader("Content-Encoding", "gzip");
    }

    // 3. Kirim Response
    request->send(response);
  });

  m_server.on("/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    String ssid = request->arg("ssid");
    String pass = request->arg("pass");

    // Input Validation
    if (ssid.length() == 0 || ssid.length() > 32) {
        request->send_P(400, "text/plain", PSTR("Invalid SSID Length"));
        return;
    }
    if (pass.length() > 64) {
        request->send_P(400, "text/plain", PSTR("Invalid Password Length"));
        return;
    }

    // Sanitize Inputs: Allow only printable ASCII (32-126)
    if (!Utils::isSafeString(ssid.c_str()) || !Utils::isSafeString(pass.c_str())) {
       request->send_P(400, "text/plain", PSTR("Invalid Characters in Credentials"));
       return;
    }
    LOG_INFO("PORTAL", F("User submitted new WiFi Credentials!"));
    LOG_INFO("PORTAL", F("SSID: %s"), ssid.c_str());
    LOG_INFO("PORTAL", F("PASS: %s"), pass.length() > 0 ? "********" : "(Empty)");
    LOG_INFO("PORTAL", F("Saving and attempting connection..."));
    if (!ConfigManager::saveTempWifiCredentials(ssid.c_str(), pass.c_str())) {
      request->send_P(500, "text/plain", PSTR("Failed to save credentials"));
      return;
    }
    m_portalStatus = PortalStatus::TESTING;
    m_portalTestTimer.reset();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    request->redirect("/connecting");
  });

  m_server.on("/connecting", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, CONNECTING_HTML_MIME, CONNECTING_HTML, CONNECTING_HTML_LEN);
    if(CONNECTING_HTML_GZIPPED) response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  m_server.on("/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    this->sendStatusJson(request);
  });
  
  // NEW: Scan for networks endpoint
  m_server.on("/networks", HTTP_GET, [this](AsyncWebServerRequest* request) {
    this->sendNetworksJson(request);
  });
  
  // NEW: Saved credentials endpoint  
  m_server.on("/saved", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "application/json", this->getSavedCredentialsJson());
  });
  
  // NEW: Forget/remove credential endpoint
  m_server.on("/forget", HTTP_POST, [this](AsyncWebServerRequest* request) {
    String ssid = request->arg("ssid");
    if (ssid.length() > 0) {
      if (m_wifiManager.removeUserCredential(ssid.c_str())) {
        request->send(200, "application/json", "{\"status\":\"ok\"}");
      } else {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Cannot remove built-in or unknown network\"}");
      }
    } else {
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing SSID\"}");
    }
  });

  m_server.on("/success", HTTP_GET, [this](AsyncWebServerRequest* request) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, REBOOTING_HTML_MIME, REBOOTING_HTML, REBOOTING_HTML_LEN);
      if (REBOOTING_HTML_GZIPPED) {
        response->addHeader("Content-Encoding", "gzip");
      }
      request->send(response);
  });

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

    int bars = 0;
    if (rssi > -50) bars = 4;
    else if (rssi > -60) bars = 3;
    else if (rssi > -70) bars = 2;
    else if (rssi > -80) bars = 1;

    bool isKnown = m_wifiManager.getCredentialStore().hasCredential(ssid.c_str());

    response->printf("{\"ssid\":\"%s\",\"rssi\":%d,\"bars\":%d,\"open\":%s,\"known\":%s}", 
      ssid.c_str(), rssi, bars, isOpen ? "true" : "false", isKnown ? "true" : "false");
  }

  response->print("]}");
  request->send(response);
}

String PortalServer::getSavedCredentialsJson() const {
  auto& store = m_wifiManager.getCredentialStore();
  String json = "{\"credentials\":[";
  bool first = true;
  
  const auto* primary = store.getPrimaryGH();
  const auto* secondary = store.getSecondaryGH();
  
  if (primary && !primary->isEmpty()) {
    json += "{\"ssid\":\"" + String(primary->ssid) + "\",\"builtin\":true,\"available\":";
    json += primary->isAvailable ? "true" : "false";
    json += "}";
    first = false;
  }
  
  if (secondary && !secondary->isEmpty()) {
    if (!first) json += ",";
    json += "{\"ssid\":\"" + String(secondary->ssid) + "\",\"builtin\":true,\"available\":";
    json += secondary->isAvailable ? "true" : "false";
    json += "}";
    first = false;
  }
  
  for (const auto& cred : store.getSavedCredentials()) {
    if (!cred.isEmpty()) {
      if (!first) json += ",";
      first = false;
      json += "{\"ssid\":\"" + String(cred.ssid) + "\",\"builtin\":false,\"available\":";
      json += cred.isAvailable ? "true" : "false";
      json += "}";
    }
  }
  
  json += "]}";
  return json;
}