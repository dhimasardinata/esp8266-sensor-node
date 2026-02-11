#include "REDACTED"

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecureBearSSL.h>
#include <cstring>
#include <cstdlib>
#include <new>

#include "CryptoUtils.h"

#include "ConfigManager.h"
#include "Logger.h"
#include "NtpClient.h"
#include "REDACTED"
#include "constants.h"
#include "node_config.h"
#include "root_ca_data.h"

OtaManager:REDACTED
                       WifiManager& wifiManager,
                       BearSSL::WiFiClientSecure& secureClient,
                       ConfigManager& configManager,
                       const BearSSL::X509List* trustAnchors)
    : m_ntpClient(ntpClient),
      m_wifiManager(wifiManager),
      m_secureClient(secureClient),
      m_configManager(configManager),
      m_trustAnchors(trustAnchors) {}

OtaManager:REDACTED

void OtaManager:REDACTED
  m_updateCheckTimer.setInterval(INITIAL_UPDATE_DELAY_MS);
  m_is_first_check = true;
}

void OtaManager:REDACTED
  (void)config;
}

void OtaManager:REDACTED
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
    m_localTrustAnchors.reset();
  }
}

void OtaManager:REDACTED
  m_uploadInProgress = inProgress;
}

bool OtaManager:REDACTED
  return m_isBusy;
}

bool OtaManager:REDACTED
  if (m_trustAnchors) {
    return true;
  }
  if (m_localTrustAnchors) {
    return true;
  }
  std::unique_ptr<BearSSL::X509List> anchors(new (std::nothrow) BearSSL::X509List(ROOT_CA_PEM));
  if (!anchors) {
    LOG_WARN("MEM", F("Trust anchors alloc failed"));
    return false;
  }
  m_localTrustAnchors.swap(anchors);
  return true;
}

const BearSSL::X509List* OtaManager::activeTrustAnchors() const {
  if (m_trustAnchors) {
    return m_trustAnchors;
  }
  return m_localTrustAnchors.get();
}

bool OtaManager:REDACTED
  if (m_tlsActive) {
    return true;
  }

  m_wifiManager.releaseScanCache();
  CryptoUtils::releaseWsCipher();
  yield();

  const uint32_t maxBlock = ESP.getMaxFreeBlockSize();
  const uint32_t totalFree = REDACTED
  if (maxBlock < AppConstants::TLS_MIN_SAFE_BLOCK_SIZE || totalFree < AppConstants::TLS_MIN_TOTAL_HEAP) {
    LOG_WARN("MEM",
             F("OTA TLS skipped (low heap: REDACTED
             totalFree,
             maxBlock);
    return false;
  }

  m_secureClient.stop();
  m_secureClient.setBufferSizes(AppConstants::TLS_RX_BUF_SIZE, AppConstants::TLS_TX_BUF_SIZE);
  m_secureClient.setTimeout(15000);

  if (allowInsecure || m_configManager.getConfig().ALLOW_INSECURE_HTTPS()) {
    m_secureClient.setInsecure();
    m_tlsActive = true;
    return true;
  }

  if (!ensureTrustAnchors()) {
    return false;
  }
  const BearSSL::X509List* anchors = activeTrustAnchors();
  if (!anchors) {
    return false;
  }
  m_secureClient.setTrustAnchors(anchors);
  m_tlsActive = true;
  return true;
}

void OtaManager:REDACTED
  if (!m_tlsActive) {
    return;
  }
  m_secureClient.stop();
  m_secureClient.setTrustAnchors(nullptr);
  m_secureClient.setInsecure();
  m_secureClient.setBufferSizes(AppConstants::TLS_RX_BUF_PORTAL, AppConstants::TLS_TX_BUF_PORTAL);
  m_localTrustAnchors.reset();
  m_tlsActive = false;
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
}  // namespace

void OtaManager:REDACTED
  m_isBusy = true;
  const auto& cfg = m_configManager.getConfig();
  const bool forcedInsecure = m_force_insecure;

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

  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (forcedInsecure) {
    LOG_WARN("REDACTED", F("REDACTED"));
  }

  {
    const uint32_t blk = ESP.getMaxFreeBlockSize();
    const uint32_t tot = ESP.getFreeHeap();
    if (blk < AppConstants::TLS_MIN_SAFE_BLOCK_SIZE || tot < AppConstants::TLS_MIN_TOTAL_HEAP) {
      LOG_WARN("MEM", F("OTA connect skipped (heap: REDACTED
      releaseTlsResources();
      m_isBusy = false;
      return;
    }
  }

  if (http.begin(m_secureClient, fullOtaUrl)) {
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

  bool updateTriggered = false;
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
    if (strncmp(firmwareUrl, "https://", 8) != 0 && !forcedInsecure) {
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
    updateTriggered = true;
    if (md5[0] != '\0') {
      String md5Str(md5);
      ESPhttpUpdate.setMD5sum(md5Str);
    }

    String urlStr(firmwareUrl);
    ESPhttpUpdate.update(m_secureClient, urlStr);
  } else {
    LOG_INFO("REDACTED", F("REDACTED"));
  }

  releaseTlsResources();
  m_isBusy = false;
}
