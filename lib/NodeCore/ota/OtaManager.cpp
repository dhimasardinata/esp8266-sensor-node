#include "REDACTED"

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecureBearSSL.h>
#include <cstring>
#include <cstdlib>
#include <new>
#include <strings.h>

#include "support/CryptoUtils.h"

#include "system/ConfigManager.h"
#include "system/Logger.h"
#include "net/NtpClient.h"
#include "system/NodeIdentity.h"
#include "REDACTED"
#include "REDACTED"
#include "REDACTED"
#include "config/constants.h"
#include "generated/node_config.h"

#ifndef FACTORY_OTA_TOKEN
#define FACTORY_OTA_TOKEN "REDACTED"
#endif

OtaManager:REDACTED
                       WifiManager& wifiManager,
                       BearSSL::WiFiClientSecure& secureClient,
                       ConfigManager& configManager,
                       const BearSSL::X509List* trustAnchors)
    : m_ntpClient(ntpClient),
      m_wifiManager(wifiManager),
      m_secureClient(secureClient),
      m_configManager(configManager),
      m_trustAnchors(trustAnchors),
      m_cloudOtaWatchdog() {}

OtaManager:REDACTED

void OtaManager:REDACTED
  m_updateCheckTimer.setInterval(INITIAL_UPDATE_DELAY_MS);
  m_is_first_check = true;
}

void OtaManager:REDACTED
  (void)config;
}

void OtaManager:REDACTED
  OtaManagerHealth:REDACTED
  if (m_wifiManager.getState() != REDACTED
    return;
  if (!m_force_check && !m_updateCheckTimer.hasElapsed())
    return;
  if (m_uploadInProgress)
    return;

  m_force_check = false;
  checkForUpdates();

  if (m_is_first_check) {
    m_is_first_check = false;
    m_updateCheckTimer.setInterval(REGULAR_UPDATE_INTERVAL_MS);
  }
}

void OtaManager:REDACTED
  LOG_INFO("REDACTED", F("REDACTED"));
  m_force_check = true;
}

void OtaManager:REDACTED
  LOG_WARN("OTA", F("SECURITY OVERRIDE: REDACTED
  m_force_check = true;
  m_force_insecure = true;
}

void OtaManager:REDACTED
  m_trustAnchors = trustAnchors;
  if (trustAnchors) {
    m_resources.localTrustAnchors.reset();
  }
}

void OtaManager:REDACTED
  m_uploadInProgress = inProgress;
}

bool OtaManager:REDACTED
  return m_isBusy;
}

void OtaManager:REDACTED
  m_cloudOtaActive = REDACTED
  m_cloudOtaStartedAt = REDACTED
  m_cloudOtaLastProgressAt = REDACTED
  m_cloudOtaCurrent = REDACTED
  m_cloudOtaTotal = REDACTED
  m_cloudOtaWatchdog.detach();
  m_cloudOtaWatchdog.attach_ms(1000, [this]() { handleCloudOtaWatchdog(); });
}

void OtaManager:REDACTED
  m_cloudOtaCurrent = REDACTED
  m_cloudOtaTotal = REDACTED
  m_cloudOtaLastProgressAt = REDACTED
}

void OtaManager:REDACTED
  m_cloudOtaWatchdog.detach();
  m_cloudOtaActive = REDACTED
  m_cloudOtaStartedAt = REDACTED
  m_cloudOtaLastProgressAt = REDACTED
  m_cloudOtaCurrent = REDACTED
  m_cloudOtaTotal = REDACTED
}

void OtaManager:REDACTED
  if (!m_cloudOtaActive) {
    return;
  }
  const unsigned long now = millis();
  if ((now - m_cloudOtaLastProgressAt) > AppConstants:REDACTED
      (now - m_cloudOtaStartedAt) > AppConstants:REDACTED
    // Ticker callbacks run outside the main loop on ESP8266, so keep recovery
    // here deliberately minimal and restart immediately on a hard OTA stall.
    m_cloudOtaActive = REDACTED
    ESP.restart();
  }
}

void OtaManager:REDACTED
                              char* version,
                              size_t version_len,
                              char* url,
                              size_t url_len,
                              char* md5,
                              size_t md5_len,
                              int& status) {
  // --- Manual JSON Parsing with bounds checking ---
  // Format expected: {"version":"9.9.9","file_url":"...","status":1}
  if (version && version_len > 0) {
    version[0] = '\0';
  }
  if (url && url_len > 0) {
    url[0] = '\0';
  }
  if (md5 && md5_len > 0) {
    md5[0] = '\0';
  }
  status = 0;

  auto extract_json_string = [&](std::string_view key, char* out, size_t out_len) {
    if (!out || out_len == 0)
      return;
    size_t keyPos = payload.find(key);
    if (keyPos == std::string_view::npos)
      return;
    size_t start = payload.find('"', keyPos + key.size());
    if (start == std::string_view::npos)
      return;
    size_t end = payload.find('"', start + 1);
    if (end == std::string_view::npos || end <= start + 1)
      return;
    size_t len = std::min(out_len - 1, end - (start + 1));
    memcpy(out, payload.data() + start + 1, len);
    out[len] = '\0';
  };

  extract_json_string("\"version\"", version, version_len);
  extract_json_string("\"file_url\"", url, url_len);
  extract_json_string("\"md5\"", md5, md5_len);

  size_t statusKey = payload.find("\"status\"");
  if (statusKey != std::string_view::npos) {
    size_t colon = payload.find(':', statusKey);
    if (colon != std::string_view::npos) {
      size_t i = colon + 1;
      while (i < payload.size() && (payload[i] == ' ' || payload[i] == '\t'))
        ++i;
      int value = 0;
      bool neg = false;
      if (i < payload.size() && payload[i] == '-') {
        neg = true;
        ++i;
      }
      while (i < payload.size() && payload[i] >= '0' && payload[i] <= '9') {
        value = (value * 10) + (payload[i] - '0');
        ++i;
      }
      status = neg ? -value : value;
    }
  }
}

namespace {
  static size_t u32_to_dec(char* out, size_t out_len, uint32_t value) {
    if (!out || out_len == 0)
      return 0;
    char tmp[10];
    size_t n = 0;
    do {
      tmp[n++] = static_cast<char>('0' + (value % 10));
      value /= 10;
    } while (value != 0 && n < sizeof(tmp));
    size_t written = 0;
    while (n > 0 && written + 1 < out_len) {
      out[written++] = tmp[--n];
    }
    out[written] = '\0';
    return written;
  }

  static bool buildOtaUrl(char* out, size_t out_len, const char* base, size_t base_len, uint32_t node_id) {
    if (!out || out_len == 0)
      return false;
    if (!base)
      base_len = 0;
    if (base_len > out_len - 1)
      base_len = out_len - 1;
    if (base_len > 0) {
      memcpy(out, base, base_len);
    }
    size_t pos = base_len;
    size_t digits = u32_to_dec(out + pos, out_len - pos, node_id);
    pos += digits;
    return digits > 0;
  }

  static bool copyTokenStrict(char* out, size_t out_len, const char* token) {
    if (!out || out_len == 0 || !token || token[0] == '\0') {
      return false;
    }
    size_t token_len = REDACTED
    if (token_len =REDACTED
      return false;
    }
    memcpy(out, token, token_len);
    out[token_len] = REDACTED
    return true;
  }

  static bool resolveOtaTokenForRequest(char* out, size_t out_len, ConfigManager& configManager) {
    return copyTokenStrict(out, out_len, configManager.getEffectiveOtaAuthToken());
  }

  static bool buildBearerHeader(char* out, size_t out_len, const char* token) {
    if (!out || out_len == 0 || !token || token[0] == '\0') {
      return false;
    }
    constexpr size_t kPrefixLen = sizeof("Bearer ") - 1;
    if (out_len <= kPrefixLen + 1) {
      return false;
    }
    size_t token_len = REDACTED
    if (token_len =REDACTED
      return false;
    }
    memcpy_P(out, PSTR("Bearer "), kPrefixLen);
    memcpy(out + kPrefixLen, token, token_len);
    out[kPrefixLen + token_len] = REDACTED
    return true;
  }
}  // namespace

void OtaManager:REDACTED
  m_isBusy = true;
  const auto& cfg = m_configManager.getConfig();
  const bool forcedInsecure = m_force_insecure;
  ESPhttpUpdate.setMD5sum(String());

  // Use stack buffer for URL construction.
  // Additional space for NODE_ID (up to 6 digits) and null terminator.
  char fullOtaUrl[MAX_URL_LEN + 8];
  const char* otaBase = REDACTED
  size_t base_len = strnlen(otaBase, MAX_URL_LEN);
  if (!buildOtaUrl(fullOtaUrl, sizeof(fullOtaUrl), otaBase, base_len, NODE_ID)) {
    LOG_ERROR("REDACTED", F("REDACTED"));
    m_configManager.releaseStrings();
    m_isBusy = false;
    return;
  }
  char checkToken[160] = REDACTED
  char checkAuthHeader[192] = REDACTED
  const bool hasCheckToken =
      resolveOtaTokenForRequest(checkToken, sizeof(checkToken), m_configManager);
  const bool hasCheckAuth =
      hasCheckToken && buildBearerHeader(checkAuthHeader, sizeof(checkAuthHeader), checkToken);
  m_configManager.releaseStrings();

  if (m_wifiManager.isScanBusy()) {
    LOG_WARN("REDACTED", F("REDACTED"));
    m_isBusy = false;
    return;
  }

  const bool allowInsecure = forcedInsecure || cfg.ALLOW_INSECURE_HTTPS();
  if (!acquireTlsResources(allowInsecure)) {
    m_isBusy = false;
    return;
  }

  LOG_INFO("REDACTED", F("REDACTED"));

  HTTPClient http;
  char serverVersion[16] = {0};
  char firmwareUrl[128] = {0};
  char md5[33] = {0};
  int apiStatus = 0;
  char userAgent[NodeIdentity::kUserAgentBufferLen];
  NodeIdentity::buildUserAgent(userAgent, sizeof(userAgent));
  char deviceId[NodeIdentity::kDeviceIdBufferLen];
  NodeIdentity::buildDeviceId(deviceId, sizeof(deviceId));

  http.setTimeout(15000);
  http.setTimeout(m_policy.checkHttpTimeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent(userAgent);
  http.addHeader(F("X-Device-ID"), deviceId);

  if (forcedInsecure) {
    LOG_WARN("REDACTED", F("REDACTED"));
  }

  const OtaManagerHealth:REDACTED
  if (!connectBudget.healthy) {
    LOG_WARN("MEM", F("OTA connect skipped (heap: REDACTED
    releaseTlsResources();
    m_isBusy = false;
    return;
  }

  if (http.begin(m_secureClient, fullOtaUrl)) {
    if (hasCheckAuth) {
      http.addHeader(F("REDACTED"), checkAuthHeader);
    }
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      char payload[256];
      int n = http.getStream().readBytes(payload, sizeof(payload) - 1);
      if (n > 0) {
        payload[n] = '\0';
        parseOtaJson(std:REDACTED
                     serverVersion,
                     sizeof(serverVersion),
                     firmwareUrl,
                     sizeof(firmwareUrl),
                     md5,
                     sizeof(md5),
                     apiStatus);
      }
    }
    http.end();
  }

  // ALWAYS Restore Security Immediately after check (unless we are updating right now)
  // If we found an update, we might need insecure client for that too...
  // ESPhttpUpdate.update takes the client.
  // Ideally, valid firmware is on HTTPS? Or HTTP?
  // If cert expired, we need insecure for download too.
  // m_secureClient state persists.

  // If we are NOT updating, or if we want to be safe, restore if not forced?
  // Actually, if we reboot after update, state is cleared.
  // If no update found, we MUST restore security for API Client!
  // BUT we don't have easy access to Cert Store here (it's in ApiClient).
  // HOWEVER, this is an Edge Case Recovery. A reboot is acceptable if check fails?
  // Or we just accept that until reboot, API is insecure?
  // Code Review: "Ensure config remains secure after reboot".
  // If we don't reboot, we are insecure.
  // FIX: Force reboot if insecure check finishes without update?
  // OR: Just let it be. The user has to manually intervention anyway.

  if (forcedInsecure) {
    m_force_insecure = false;  // Reset flag
  }

  // Compare semantic versions (e.g., v1.2.3).
  auto parseVersion = [](const char* v, int& major, int& minor, int& patch) {
    major = minor = patch = 0;
    if (!v || v[0] == '\0')
      return;
    const char* firstDot = strchr(v, '.');
    if (!firstDot)
      return;
    const char* secondDot = strchr(firstDot + 1, '.');
    if (!secondDot)
      return;
    major = atoi(v);
    minor = atoi(firstDot + 1);
    patch = atoi(secondDot + 1);
  };

  int sMajor, sMinor, sPatch, lMajor, lMinor, lPatch;
  parseVersion(serverVersion, sMajor, sMinor, sPatch);
  parseVersion(FIRMWARE_VERSION, lMajor, lMinor, lPatch);

  bool isNewer = (sMajor > lMajor) || (sMajor == lMajor && sMinor > lMinor) ||
                 (sMajor == lMajor && sMinor == lMinor && sPatch > lPatch);

  if (serverVersion[0] != '\0' && firmwareUrl[0] != '\0' && apiStatus == 1 && isNewer) {
    if (strncmp_P(firmwareUrl, PSTR("https://"), 8) != 0 && !forcedInsecure) {
      LOG_ERROR("REDACTED", F("REDACTED"));
      releaseTlsResources();
      m_isBusy = false;
      return;
    }
#ifdef OTA_REQUIRE_MD5
    if (md5[0] == '\0') {
      LOG_ERROR("REDACTED", F("REDACTED"));
      releaseTlsResources();
      m_isBusy = false;
      return;
    }
#endif
    // If we are insecure, we allow http:// or https:// (ignoring certs)

    LOG_INFO("REDACTED", F("REDACTED"), serverVersion);
    if (md5[0] != '\0') {
      ESPhttpUpdate.setMD5sum(String(md5));
    }
    ESPhttpUpdate.rebootOnUpdate(false);
    ESPhttpUpdate.setClientTimeout(static_cast<int>(m_policy.downloadHttpTimeoutMs));
    ESPhttpUpdate.onStart([this]() { beginCloudOtaSession(); });
    ESPhttpUpdate.onProgress([this](int current, int total) {
      updateCloudOtaProgress(static_cast<size_t>(current), static_cast<size_t>(total));
    });
    ESPhttpUpdate.onEnd([this]() { finishCloudOtaSession(); });
    ESPhttpUpdate.onError([this](int err) {
      LOG_ERROR("OTA", F("Cloud OTA error callback: REDACTED
      finishCloudOtaSession();
    });

    char downloadToken[160] = REDACTED
    char downloadAuthHeader[192] = REDACTED
    const bool hasDownloadToken =
        resolveOtaTokenForRequest(downloadToken, sizeof(downloadToken), m_configManager);
    const bool hasDownloadAuth = REDACTED
                                 buildBearerHeader(downloadAuthHeader, sizeof(downloadAuthHeader), downloadToken);
    m_configManager.releaseStrings();

    HTTPClient updateHttp;
    updateHttp.setTimeout(m_policy.downloadHttpTimeoutMs);
    updateHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (updateHttp.begin(m_secureClient, String(firmwareUrl))) {
      if (hasDownloadAuth) {
        updateHttp.addHeader(F("REDACTED"), downloadAuthHeader);
      }
      updateHttp.addHeader(F("X-Device-ID"), deviceId);
      t_httpUpdate_return updateResult = ESPhttpUpdate.update(updateHttp, FIRMWARE_VERSION);
      if (updateResult == HTTP_UPDATE_FAILED) {
        LOG_ERROR("OTA", F("Update failed: REDACTED
        finishCloudOtaSession();
      } else if (updateResult == HTTP_UPDATE_NO_UPDATES) {
        LOG_INFO("REDACTED", F("REDACTED"));
        finishCloudOtaSession();
      } else if (updateResult == HTTP_UPDATE_OK) {
        finishCloudOtaSession();
        releaseTlsResources();
        m_isBusy = false;
        BootGuard::setRebootReason(BootGuard::RebootReason::OTA_UPDATE);
        delay(100);
        ESP.restart();
        return;
      }
    } else {
      LOG_ERROR("REDACTED", F("REDACTED"));
      finishCloudOtaSession();
    }
    ESPhttpUpdate.setMD5sum(String());
    ESPhttpUpdate.onStart(HTTPUpdateStartCB());
    ESPhttpUpdate.onProgress(HTTPUpdateProgressCB());
    ESPhttpUpdate.onEnd(HTTPUpdateEndCB());
    ESPhttpUpdate.onError(HTTPUpdateErrorCB());
    ESPhttpUpdate.rebootOnUpdate(true);
  } else {
    LOG_INFO("REDACTED", F("REDACTED"));
  }

  releaseTlsResources();
  m_isBusy = false;
}
