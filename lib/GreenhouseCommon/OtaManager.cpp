#include "OtaManager.h"

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecureBearSSL.h>

#include "ConfigManager.h"
#include "NtpClient.h"
#include "WifiManager.h"
#include "node_config.h"
#include "root_ca_data.h"
#include "Logger.h"

OtaManager::OtaManager(NtpClient& ntpClient,
                       WifiManager& wifiManager,
                       BearSSL::WiFiClientSecure& secureClient,
                       ConfigManager& configManager)
    : m_ntpClient(ntpClient),
      m_wifiManager(wifiManager),
      m_secureClient(secureClient),
      m_configManager(configManager) {}

void OtaManager::init() {
  m_updateCheckTimer.setInterval(INITIAL_UPDATE_DELAY_MS);
  m_is_first_check = true;
}

void OtaManager::applyConfig(const AppConfig& config) {
  (void)config;
}

void OtaManager::handle() {
  if (m_wifiManager.getState() != WifiManager::State::CONNECTED_STA || !m_ntpClient.isTimeSynced()) return;
  if (!m_force_check && !m_updateCheckTimer.hasElapsed()) return;

  m_force_check = false;
  checkForUpdates();

  if (m_is_first_check) {
    m_is_first_check = false;
    m_updateCheckTimer.setInterval(REGULAR_UPDATE_INTERVAL_MS);
  }
}

void OtaManager::forceUpdateCheck() {
  LOG_INFO("OTA", F("Manual check scheduled."));
  m_force_check = true;
}

void OtaManager::parseOtaJson(const String& payload, String& version, String& url, int& status) {
  // --- Manual JSON Parsing (Simple) ---
  // Format expected: {"version":"9.9.9","file_url":"...","status":1}

  // Extract Version
  int verKey = payload.indexOf("\"version\"");
  if (verKey != -1) {
    int verStart = payload.indexOf("\"", verKey + 9) + 1;
    int verEnd = payload.indexOf("\"", verStart);
    version = payload.substring(verStart, verEnd);
  }

  // Extract URL
  int urlKey = payload.indexOf("\"file_url\"");
  if (urlKey != -1) {
    int urlStart = payload.indexOf("\"", urlKey + 10) + 1;
    int urlEnd = payload.indexOf("\"", urlStart);
    url = payload.substring(urlStart, urlEnd);
  }

  // Extract Status
  int statusKey = payload.indexOf("\"status\"");
  if (statusKey != -1) status = payload.substring(payload.indexOf(":", statusKey) + 1).toInt();
}

void OtaManager::checkForUpdates() {
  const auto& cfg = m_configManager.getConfig();
  
  // OPTIMIZATION: Use stack buffer instead of String concatenation
  // +8 for NODE_ID (up to 6 digits) and null terminator
  char fullOtaUrl[MAX_URL_LEN + 8];
  snprintf(fullOtaUrl, sizeof(fullOtaUrl), "%s%d", cfg.FW_VERSION_CHECK_URL_BASE.data(), NODE_ID);
  
  LOG_INFO("OTA", F("Checking..."));

  HTTPClient http;
  String serverVersion, firmwareUrl;
  int apiStatus = 0;

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (http.begin(m_secureClient, fullOtaUrl)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      parseOtaJson(payload, serverVersion, firmwareUrl, apiStatus);
    }
    http.end();
  }

  // Semantic version comparison (v1.2.3)
  auto parseVersion = [](const String& v, int& major, int& minor, int& patch) {
    int firstDot = v.indexOf('.');
    int secondDot = v.indexOf('.', firstDot + 1);
    major = v.substring(0, firstDot).toInt();
    minor = v.substring(firstDot + 1, secondDot).toInt();
    patch = v.substring(secondDot + 1).toInt();
  };

  int sMajor, sMinor, sPatch, lMajor, lMinor, lPatch;
  parseVersion(serverVersion, sMajor, sMinor, sPatch);
  parseVersion(FIRMWARE_VERSION, lMajor, lMinor, lPatch);

  bool isNewer = (sMajor > lMajor) || 
                 (sMajor == lMajor && sMinor > lMinor) || 
                 (sMajor == lMajor && sMinor == lMinor && sPatch > lPatch);

  if (!serverVersion.isEmpty() && !firmwareUrl.isEmpty() && apiStatus == 1 && isNewer) {
    if (!firmwareUrl.startsWith("https://")) {
      LOG_ERROR("OTA-SEC", F("Blocked non-HTTPS firmware URL. Aborting."));
      return;
    }
    LOG_INFO("OTA", F("New firmware found (v%s). Updating..."), serverVersion.c_str());
    ESPhttpUpdate.update(m_secureClient, firmwareUrl);
  } else {
    LOG_INFO("OTA", F("Up to date."));
  }
}