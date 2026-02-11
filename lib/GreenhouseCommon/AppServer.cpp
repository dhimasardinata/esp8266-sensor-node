#include "AppServer.h"

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>

#include "ConfigManager.h"
#include "Logger.h"

AppServer::AppServer(AsyncWebServer& server,
                     AsyncWebSocket& ws,
                     ConfigManager& configManager,
                     SensorManager& sensorManager,
                     WifiManager& wifiManager)
    : m_server(server),
      m_ws(ws),
      m_configManager(configManager),
      m_sensorManager(sensorManager),
      m_wifiManager(wifiManager) {}

void AppServer::onWifiStateChanged(WifiManager::State newState) {
  if (newState == WifiManager::State::CONNECTED_STA) {
    if (!m_isRunning) {
      LOG_INFO("REDACTED", F("REDACTED"));
      begin();
    }
  } else {
    if (m_isRunning) {
      LOG_INFO("REDACTED", F("REDACTED"));
      stop();
    }
  }
}

void AppServer::handle() {
  if (m_isRunning) {
    unsigned long now = millis();
    if (!m_mdnsStarted && WiFi.status() =REDACTED
      m_lastMdnsStartAttempt = now;
      char hostname[32];
      m_configManager.getHostname(hostname, sizeof(hostname));
      if (MDNS.begin(hostname)) {
        MDNS.addService("http", "tcp", 80);
        m_mdnsStarted = true;
        LOG_INFO("mDNS", F("Responder started: http://%s.local"), hostname);
      } else {
        LOG_WARN("mDNS", F("Responder start failed. Will retry."));
      }
    }

    if (m_mdnsStarted && (now - m_lastMdns >= 1000UL)) {
      m_lastMdns = now;
      // Keep mDNS responsive even under low heap; update is lightweight.
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

  // Start mDNS responder.
  m_mdnsStarted = false;
  m_lastMdnsStartAttempt = millis();
  if (MDNS.begin(hostname)) {
    MDNS.addService("http", "tcp", 80);
    m_mdnsStarted = true;
    LOG_INFO("mDNS", F("Responder started: http://%s.local"), hostname);
  } else {
    LOG_ERROR("mDNS", F("Error setting up MDNS responder!"));
  }

  // Initialize ArduinoOTA.
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
  ArduinoOTA.end();
  if (m_mdnsStarted) {
    MDNS.close();
    m_mdnsStarted = false;
  }
  m_isRunning = false;
}

void AppServer::onFlashRequest(std::function<void()> fn) {
  m_flash_request_callback = fn;
}

void AppServer::setOtaCallbacks(std::function<void()> onStart, std::function<void()> onEnd) {
  m_otaStartCallback = REDACTED
  m_otaEndCallback = REDACTED
}

