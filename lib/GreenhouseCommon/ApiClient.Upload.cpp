#include "ApiClient.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <bearssl/bearssl_hmac.h>
#include <user_interface.h>

#include <algorithm>
#include <array>

#include "CacheManager.h"  // Concrete type for CRTP
#include "CompileTimeJSON.h"
#include "ConfigManager.h"
#include "CryptoUtils.h"
#include "Logger.h"
#include "NtpClient.h"
#include "SensorManager.h"  // Concrete type for CRTP
#include "REDACTED"
#include "constants.h"
#include "node_config.h"
#include "sensor_data.h"
#include "utils.h"
#include "RtcManager.h"

// ApiClient.Upload.cpp - payload creation and upload policy

namespace {
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
  static constexpr char kGatewayDataUrl[] = "http://gateway-gh-" STR(GH_ID) ".local/api/data";
  static constexpr char kGatewayModeUrl[] = "http://gateway-gh-" STR(GH_ID) ".local/api/mode";
  static constexpr char kGatewayDataPath[] = "/api/data";
  static constexpr char kGatewayModePath[] = "/api/mode";
#undef STR
#undef STR_HELPER

  static int32_t round_to_int(float v) {
    return (v >= 0.0f) ? static_cast<int32_t>(v + 0.5f) : static_cast<int32_t>(v - 0.5f);
  }

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

  static size_t append_literal(char* out, size_t out_len, size_t pos, const char* text) {
    if (!out || pos >= out_len || !text)
      return pos;
    size_t remaining = out_len - pos;
    if (remaining <= 1)
      return pos;
    size_t n = strnlen(text, remaining - 1);
    memcpy(out + pos, text, n);
    pos += n;
    out[pos] = '\0';
    return pos;
  }

  static size_t append_u32(char* out, size_t out_len, size_t pos, uint32_t value) {
    if (!out || pos >= out_len)
      return pos;
    size_t remaining = out_len - pos;
    if (remaining <= 1)
      return pos;
    size_t n = u32_to_dec(out + pos, remaining, value);
    pos += n;
    if (pos < out_len)
      out[pos] = '\0';
    return pos;
  }

  static size_t append_i32(char* out, size_t out_len, size_t pos, int32_t value) {
    if (value < 0) {
      pos = append_literal(out, out_len, pos, "-");
      value = -value;
    }
    return append_u32(out, out_len, pos, static_cast<uint32_t>(value));
  }

  static size_t append_cstr(char* out, size_t out_len, size_t pos, const char* text) {
    return append_literal(out, out_len, pos, text);
  }

  static bool append_bytes_strict(char* out, size_t out_len, size_t& pos, const char* text, size_t text_len) {
    if (!out || !text)
      return false;
    if (pos + text_len >= out_len)
      return false;
    if (text_len > 0) {
      memcpy(out + pos, text, text_len);
      pos += text_len;
      out[pos] = '\0';
    }
    return true;
  }

  static bool append_char_strict(char* out, size_t out_len, size_t& pos, char c) {
    if (!out || pos + 1 >= out_len)
      return false;
    out[pos++] = c;
    out[pos] = '\0';
    return true;
  }

  static bool append_u32_strict(char* out, size_t out_len, size_t& pos, uint32_t value) {
    char tmp[10];
    size_t n = u32_to_dec(tmp, sizeof(tmp), value);
    return append_bytes_strict(out, out_len, pos, tmp, n);
  }

  static bool append_http_prefix(char* out, size_t out_len, size_t& pos) {
    static constexpr char kHttpPrefix[] = "http://";
    return append_bytes_strict(out, out_len, pos, kHttpPrefix, sizeof(kHttpPrefix) - 1);
  }

  static bool build_gateway_url_from_host_str(char* out, size_t out_len, const char* host, const char* path) {
    if (!out || out_len == 0 || !host || !path)
      return false;
    if (host[0] == '\0')
      return false;
    size_t pos = 0;
    if (!append_http_prefix(out, out_len, pos))
      return false;
    pos = append_cstr(out, out_len, pos, host);
    if (pos == 0 || pos >= out_len)
      return false;
    return append_bytes_strict(out, out_len, pos, path, strnlen(path, out_len - pos - 1));
  }

  static bool build_gateway_url_from_ip_str(char* out, size_t out_len, const char* ip, const char* path) {
    return build_gateway_url_from_host_str(out, out_len, ip, path);
  }

  static bool append_gateway_candidate(char urls[][MAX_URL_LEN],
                                       size_t max_urls,
                                       size_t& count,
                                       const char* candidate) {
    if (!urls || max_urls == 0 || !candidate || candidate[0] == '\0') {
      return false;
    }
    for (size_t i = 0; i < count; ++i) {
      if (strncmp(urls[i], candidate, MAX_URL_LEN) == 0) {
        return false;
      }
    }
    if (count >= max_urls) {
      return false;
    }
    size_t n = strnlen(candidate, MAX_URL_LEN - 1);
    memcpy(urls[count], candidate, n);
    urls[count][n] = '\0';
    ++count;
    return true;
  }

  static size_t build_gateway_url_candidates(char urls[][MAX_URL_LEN],
                                             size_t max_urls,
                                             const char* path,
                                             const char* mdns_url) {
    if (!urls || max_urls == 0 || !path) {
      return 0;
    }
    for (size_t i = 0; i < max_urls; ++i) {
      urls[i][0] = '\0';
    }

    size_t count = 0;
    const bool preferGh1 =
        (GH_ID == 1) ? (NODE_ID <= 5)
        : (GH_ID == 2) ? false
                       : (NODE_ID <= 5);
    static constexpr char kGatewayGh1[] = "gateway-gh-1.local";
    static constexpr char kGatewayGh2[] = "gateway-gh-2.local";
    const char* primaryMdnsHost = preferGh1 ? kGatewayGh1 : kGatewayGh2;
    const char* secondaryMdnsHost = preferGh1 ? kGatewayGh2 : kGatewayGh1;
    const char* primaryIp = preferGh1 ? DEFAULT_GATEWAY_IP_GH1 : DEFAULT_GATEWAY_IP_GH2;
    const char* secondaryIp = preferGh1 ? DEFAULT_GATEWAY_IP_GH2 : DEFAULT_GATEWAY_IP_GH1;

    // Order must stay: primary mDNS -> primary IP -> secondary mDNS -> secondary IP
    if (mdns_url && mdns_url[0] != '\0') {
      (void)append_gateway_candidate(urls, max_urls, count, mdns_url);
    } else {
      char mdnsUrl[MAX_URL_LEN] = {0};
      if (build_gateway_url_from_host_str(mdnsUrl, sizeof(mdnsUrl), primaryMdnsHost, path)) {
        (void)append_gateway_candidate(urls, max_urls, count, mdnsUrl);
      }
    }

    char ipUrl[MAX_URL_LEN] = {0};
    if (build_gateway_url_from_ip_str(ipUrl, sizeof(ipUrl), primaryIp, path)) {
      (void)append_gateway_candidate(urls, max_urls, count, ipUrl);
    }

    char secondaryMdnsUrl[MAX_URL_LEN] = {0};
    if (build_gateway_url_from_host_str(
            secondaryMdnsUrl, sizeof(secondaryMdnsUrl), secondaryMdnsHost, path)) {
      (void)append_gateway_candidate(urls, max_urls, count, secondaryMdnsUrl);
    }

    if (build_gateway_url_from_ip_str(ipUrl, sizeof(ipUrl), secondaryIp, path)) {
      (void)append_gateway_candidate(urls, max_urls, count, ipUrl);
    }
    return count;
  }

  static bool build_gateway_url(char* out, size_t out_len, const char* path, const char* mdns_url) {
    if (!out || out_len == 0 || !path)
      return false;
    out[0] = '\0';
    char candidates[4][MAX_URL_LEN] = {{0}};
    size_t count = build_gateway_url_candidates(candidates, 4, path, mdns_url);
    if (count == 0) {
      return false;
    }

    size_t n = strnlen(candidates[0], out_len - 1);
    memcpy(out, candidates[0], n);
    out[n] = '\0';
    return n > 0;
  }

  static bool append_i32_strict(char* out, size_t out_len, size_t& pos, int32_t value) {
    if (value < 0) {
      if (!append_char_strict(out, out_len, pos, '-'))
        return false;
      value = -value;
    }
    return append_u32_strict(out, out_len, pos, static_cast<uint32_t>(value));
  }

  static bool append_fixed1_strict(char* out, size_t out_len, size_t& pos, int32_t value10) {
    if (value10 < 0) {
      if (!append_char_strict(out, out_len, pos, '-'))
        return false;
      value10 = -value10;
    }
    uint32_t int_part = static_cast<uint32_t>(value10 / 10);
    uint32_t frac = static_cast<uint32_t>(value10 % 10);
    if (!append_u32_strict(out, out_len, pos, int_part))
      return false;
    if (!append_char_strict(out, out_len, pos, '.'))
      return false;
    return append_char_strict(out, out_len, pos, static_cast<char>('0' + frac));
  }

  static bool ssid_equals(const char* a, const char* b) {
    if (!a || !b) {
      return false;
    }
    return strncmp(a, b, WIFI_SSID_MAX_LEN) =REDACTED
  }

  static int16_t resolve_nonactive_rssi(WifiManager& wifiManager) {
    auto& store = wifiManager.getCredentialStore();
    const WifiCredential* primary = REDACTED
    const WifiCredential* secondary = REDACTED
    if (!primary || !secondary) {
      return -100;
    }

    char activeSsid[WIFI_SSID_MAX_LEN] = REDACTED
    if (WiFi.isConnected()) {
      WiFi.SSID().toCharArray(activeSsid, sizeof(activeSsid));
    }

    if (ssid_equals(activeSsid, primary->ssid)) {
      return secondary->lastRssi;
    }
    if (ssid_equals(activeSsid, secondary->ssid)) {
      return primary->lastRssi;
    }
    return -100;
  }

  static bool extract_recorded_at_value(const char* payload,
                                        char* out,
                                        size_t out_len,
                                        size_t& value_len) {
    if (!payload || !out || out_len == 0) {
      return false;
    }
    static constexpr char kRecordedPrefix[] = ",\"recorded_at\":\"";
    const char* start = strstr(payload, kRecordedPrefix);
    if (!start) {
      return false;
    }

    const char* valueStart = start + (sizeof(kRecordedPrefix) - 1);
    const char* endQuote = strchr(valueStart, '"');
    if (!endQuote) {
      return false;
    }

    const size_t len = static_cast<size_t>(endQuote - valueStart);
    if (len >= out_len) {
      return false;
    }

    if (len > 0) {
      memcpy(out, valueStart, len);
    }
    out[len] = '\0';
    value_len = len;
    return true;
  }

  static bool strip_recorded_at_field(char* payload, size_t& len) {
    if (!payload || len == 0) {
      return false;
    }
    static constexpr char kRecordedPrefix[] = ",\"recorded_at\":\"";
    char* start = strstr(payload, kRecordedPrefix);
    if (!start) {
      return true;  // nothing to strip
    }

    char* valueStart = start + (sizeof(kRecordedPrefix) - 1);
    char* endQuote = strchr(valueStart, '"');
    if (!endQuote) {
      return false;
    }

    char* removeEnd = endQuote + 1;
    const size_t removeLen = static_cast<size_t>(removeEnd - start);
    const size_t tailLen = strnlen(removeEnd, len - static_cast<size_t>(removeEnd - payload)) + 1;
    memmove(start, removeEnd, tailLen);
    if (len >= removeLen) {
      len -= removeLen;
    } else {
      len = strnlen(payload, MAX_PAYLOAD_SIZE);
    }
    return true;
  }

  static size_t buildSensorPayload(char* out,
                                   size_t out_len,
                                   uint32_t gh_id,
                                   uint32_t node_id,
                                   int32_t temp10,
                                   int32_t hum10,
                                   uint32_t lux,
                                   int32_t rssi,
                                   const char* timeStr,
                                   size_t timeLen) {
    if (!out || out_len == 0 || !timeStr)
      return 0;
    out[0] = '\0';
    size_t pos = 0;
    static constexpr char kGhId[] = "{\"gh_id\":";
    static constexpr char kNodeId[] = ",\"node_id\":";
    static constexpr char kTemp[] = ",\"temperature\":";
    static constexpr char kHum[] = ",\"humidity\":";
    static constexpr char kLux[] = ",\"light_intensity\":";
    static constexpr char kRssi[] = ",\"rssi\":";
    static constexpr char kTime[] = ",\"recorded_at\":\"";
    static constexpr char kEnd[] = "\"}";

    if (!append_bytes_strict(out, out_len, pos, kGhId, sizeof(kGhId) - 1))
      return 0;
    if (!append_u32_strict(out, out_len, pos, gh_id))
      return 0;
    if (!append_bytes_strict(out, out_len, pos, kNodeId, sizeof(kNodeId) - 1))
      return 0;
    if (!append_u32_strict(out, out_len, pos, node_id))
      return 0;
    if (!append_bytes_strict(out, out_len, pos, kTemp, sizeof(kTemp) - 1))
      return 0;
    if (!append_fixed1_strict(out, out_len, pos, temp10))
      return 0;
    if (!append_bytes_strict(out, out_len, pos, kHum, sizeof(kHum) - 1))
      return 0;
    if (!append_fixed1_strict(out, out_len, pos, hum10))
      return 0;
    if (!append_bytes_strict(out, out_len, pos, kLux, sizeof(kLux) - 1))
      return 0;
    if (!append_u32_strict(out, out_len, pos, lux))
      return 0;
    if (!append_bytes_strict(out, out_len, pos, kRssi, sizeof(kRssi) - 1))
      return 0;
    if (!append_i32_strict(out, out_len, pos, rssi))
      return 0;
    if (!append_bytes_strict(out, out_len, pos, kTime, sizeof(kTime) - 1))
      return 0;
    if (!append_bytes_strict(out, out_len, pos, timeStr, timeLen))
      return 0;
    if (!append_bytes_strict(out, out_len, pos, kEnd, sizeof(kEnd) - 1))
      return 0;
    return pos;
  }

  static void format_datetime(char* out, size_t out_len, const tm& t) {
    if (!out || out_len < 20) {
      if (out && out_len > 0)
        out[0] = '\0';
      return;
    }
    uint16_t year = static_cast<uint16_t>(t.tm_year + 1900);
    uint8_t mon = static_cast<uint8_t>(t.tm_mon + 1);
    uint8_t day = static_cast<uint8_t>(t.tm_mday);
    uint8_t hour = static_cast<uint8_t>(t.tm_hour);
    uint8_t min = static_cast<uint8_t>(t.tm_min);
    uint8_t sec = static_cast<uint8_t>(t.tm_sec);

    out[0] = static_cast<char>('0' + (year / 1000) % 10);
    out[1] = static_cast<char>('0' + (year / 100) % 10);
    out[2] = static_cast<char>('0' + (year / 10) % 10);
    out[3] = static_cast<char>('0' + (year % 10));
    out[4] = '-';
    out[5] = static_cast<char>('0' + (mon / 10));
    out[6] = static_cast<char>('0' + (mon % 10));
    out[7] = '-';
    out[8] = static_cast<char>('0' + (day / 10));
    out[9] = static_cast<char>('0' + (day % 10));
    out[10] = ' ';
    out[11] = static_cast<char>('0' + (hour / 10));
    out[12] = static_cast<char>('0' + (hour % 10));
    out[13] = ':';
    out[14] = static_cast<char>('0' + (min / 10));
    out[15] = static_cast<char>('0' + (min % 10));
    out[16] = ':';
    out[17] = static_cast<char>('0' + (sec / 10));
    out[18] = static_cast<char>('0' + (sec % 10));
    out[19] = '\0';
  }
}  // namespace

void ApiClient::buildLocalGatewayUrl(char* buffer, size_t bufferSize) {
  if (!buffer || bufferSize == 0)
    return;
  if (!build_gateway_url(buffer, bufferSize, kGatewayDataPath, kGatewayDataUrl)) {
    buffer[0] = '\0';
  }
}

UploadResult ApiClient::performLocalGatewayUpload(const char* payload, size_t length) {
  UploadResult result = {HTTPC_ERROR_CONNECTION_FAILED, false, "Gateway Fail"};
  if (!payload || length == 0) {
    result.httpCode = HTTPC_ERROR_CONNECTION_FAILED;
    strncpy(result.message, "No payload", sizeof(result.message) - 1);
    result.message[sizeof(result.message) - 1] = '\0';
    return result;
  }

  if (m_wifiManager.isScanBusy()) {
    result.httpCode = HTTPC_ERROR_CONNECTION_LOST;
    strncpy(result.message, "REDACTED", sizeof(result.message) - 1);
    result.message[sizeof(result.message) - 1] = '\0';
    return result;
  }

  if (!m_httpClient) {
    m_httpClient.reset(new (std::nothrow) HTTPClient());
  }
  if (!m_httpClient) {
    result.httpCode = HTTPC_ERROR_TOO_LESS_RAM;
    strncpy(result.message, "HTTP alloc fail", sizeof(result.message) - 1);
    result.message[sizeof(result.message) - 1] = '\0';
    return result;
  }

  char gatewayUrls[4][MAX_URL_LEN] = {{0}};
  const size_t gatewayUrlCount =
      build_gateway_url_candidates(gatewayUrls, 4, kGatewayDataPath, kGatewayDataUrl);
  if (gatewayUrlCount == 0) {
    result.httpCode = HTTPC_ERROR_CONNECTION_FAILED;
    strncpy(result.message, "Gateway URL fail", sizeof(result.message) - 1);
    result.message[sizeof(result.message) - 1] = '\0';
    return result;
  }

  m_httpClient->setReuse(false);
  m_httpClient->setTimeout(2000);
  for (size_t i = 0; i < gatewayUrlCount; ++i) {
    const char* gatewayUrl = gatewayUrls[i];
    if (!gatewayUrl || gatewayUrl[0] == '\0') {
      continue;
    }

    if (!m_httpClient->begin(m_plainClient, gatewayUrl)) {
      result.httpCode = HTTPC_ERROR_CONNECTION_FAILED;
      strncpy(result.message, "Gateway begin fail", sizeof(result.message) - 1);
      result.message[sizeof(result.message) - 1] = '\0';
      continue;
    }

    m_httpClient->addHeader("Content-Type", "application/json");
    const int httpCode = m_httpClient->POST(reinterpret_cast<const uint8_t*>(payload), length);
    result.httpCode = static_cast<int16_t>(httpCode);
    result.success = (httpCode >= 200 && httpCode < 300);
    if (result.success) {
      strncpy(result.message, "OK (Edge)", sizeof(result.message) - 1);
      result.message[sizeof(result.message) - 1] = '\0';
      m_httpClient->end();
      return result;
    }
    snprintf(result.message, sizeof(result.message), "HTTP %d", httpCode);
    m_httpClient->end();
  }

  return result;
}


void ApiClient::notifyLowMemory(uint32_t maxBlock, uint32_t totalFree) {
  LOG_WARN("MEM", F("Low Mem - Skip. Block: %u, Total: %u"), maxBlock, totalFree);
  char msg[80];
  msg[0] = '\0';
  size_t pos = 0;
  pos = append_literal(msg, sizeof(msg), pos, "[SYSTEM] Upload Skipped: Low RAM (Free: ");
  pos = append_u32(msg, sizeof(msg), pos, totalFree);
  pos = append_literal(msg, sizeof(msg), pos, ", Blk: ");
  pos = append_u32(msg, sizeof(msg), pos, maxBlock);
  pos = append_literal(msg, sizeof(msg), pos, ")");
  if (pos > 0) {
    broadcastEncrypted(std::string_view(msg, pos));
  }
}


unsigned long ApiClient::calculateBackoffInterval(const AppConfig& cfg) {
  unsigned long multiplier = 1;
  if (m_consecutiveUploadFailures < 16) {
    multiplier = (1UL << m_consecutiveUploadFailures);
  } else {
    multiplier = (1UL << 15);
  }

  unsigned long nextInterval = cfg.CACHE_SEND_INTERVAL_MS * multiplier;
  return (nextInterval > MAX_BACKOFF_MS) ? MAX_BACKOFF_MS : nextInterval;
}

// Centralized connection health tracking

void ApiClient::trackUploadFailure() {
  m_consecutiveUploadFailures++;

  if (m_consecutiveUploadFailures == 5) {
    LOG_WARN("REDACTED", F("REDACTED"));
    WiFi.disconnect(false);
    // WiFiManager (autoConnect) or main loop should handle reconnection
  }
}


void ApiClient::handleSuccessfulUpload(UploadResult& res, const AppConfig& cfg) {
  const UploadRecordSource uploadedFrom = m_loadedRecordSource;
  LOG_INFO("UPLOAD", F("Success: HTTP %d (%s)"), res.httpCode, res.message);
  m_lastApiSuccessMillis = millis();
  m_swWdtTimer.reset();
  m_currentRecordSentToGateway = false;
  m_forceCloudAfterEdgeFailure = false;

  if (m_consecutiveUploadFailures > 0) {
    m_consecutiveUploadFailures = 0;
    m_cacheSendTimer.setInterval(cfg.CACHE_SEND_INTERVAL_MS);
    LOG_INFO("UPLOAD", F("Backoff reset to normal interval."));
  }

  // RESTORED AUTO MODE LOGIC: If a cloud upload succeeds, clear gateway fallback
  if (m_localGatewayMode && !m_targetIsEdge) {
    m_localGatewayMode = false;
    LOG_INFO("UPLOAD", F("Cloud recovered! Exiting gateway mode."));
    broadcastEncrypted("[SYSTEM] Cloud API recovered. Normal mode restored.");
  }

  if (popLoadedRecord()) {
    if (m_queuePopFailStreak > 0 || m_queuePopRetryAfter != 0) {
      m_cacheSendTimer.setInterval(cfg.CACHE_SEND_INTERVAL_MS);
    }
    m_queuePopFailStreak = 0;
    m_queuePopRetryAfter = 0;
    char msg[80];
    msg[0] = '\0';
    size_t pos = 0;
    pos = append_literal(msg, sizeof(msg), pos, "[SYSTEM] Upload OK (HTTP ");
    pos = append_i32(msg, sizeof(msg), pos, res.httpCode);
    pos = append_literal(msg, sizeof(msg), pos, ") via ");
    pos = append_literal(msg,
                         sizeof(msg),
                         pos,
                         uploadedFrom == UploadRecordSource::RTC ? "RTC"
                         : uploadedFrom == UploadRecordSource::LITTLEFS ? "LittleFS"
                                                                        : "Unknown");
    if (pos > 0) {
      broadcastEncrypted(std::string_view(msg, pos));
    }
    m_loadedRecordSource = UploadRecordSource::NONE;
  } else {
    m_uploadState = UploadState::IDLE;
    applyQueuePopFailureCooldown(cfg,
                                 uploadedFrom == UploadRecordSource::RTC ? "RTC"
                                 : uploadedFrom == UploadRecordSource::LITTLEFS ? "LittleFS"
                                                                                : "Unknown");
  }
}


void ApiClient::handleFailedUpload(UploadResult& res, const AppConfig& cfg) {
  LOG_WARN("UPLOAD", F("Failed: %d (%s)"), res.httpCode, res.message);
  char msg[80];
  msg[0] = '\0';
  size_t pos = 0;
  pos = append_literal(msg, sizeof(msg), pos, "[SYSTEM] Fail: ");
  pos = append_cstr(msg, sizeof(msg), pos, res.message);
  pos = append_literal(msg, sizeof(msg), pos, " (");
  pos = append_i32(msg, sizeof(msg), pos, res.httpCode);
  pos = append_literal(msg, sizeof(msg), pos, ")");
  if (pos > 0) {
    broadcastEncrypted(std::string_view(msg, pos));
  }

  m_uploadState = UploadState::IDLE;
  m_cacheSendTimer.reset();

  // Centralized failure tracking (Inc counter + Toggle WiFi if needed)
  trackUploadFailure();

  unsigned long nextInterval = calculateBackoffInterval(cfg);
  m_cacheSendTimer.setInterval(nextInterval);

  LOG_WARN("UPLOAD",
           F("Backoff active. Failures: %u. Next retry in: %lu s"),
           m_consecutiveUploadFailures,
           nextInterval / 1000);

  // RESTORED AUTO MODE LOGIC: Fallback to Gateway if threshold reached
  if (m_uploadMode == UploadMode::AUTO && !m_localGatewayMode) {
    if (m_consecutiveUploadFailures >= AppConstants::LOCAL_GATEWAY_FALLBACK_THRESHOLD) {
      m_localGatewayMode = true;
      m_lastCloudRetryAttempt = millis();
      LOG_WARN("UPLOAD", F("Cloud unreachable. Switching to Gateway mode."));
      broadcastEncrypted("[SYSTEM] Cloud unreachable. Gateway mode active.");
    }
  }
}


bool ApiClient::isHeapHealthy() {
  uint32_t maxBlock = ESP.getMaxFreeBlockSize();
  uint32_t totalFree = REDACTED
  if (maxBlock < AppConstants::API_MIN_SAFE_BLOCK_SIZE || totalFree < AppConstants::API_MIN_TOTAL_HEAP) {
    LOG_WARN("MEM", F("Low Mem - Skip. Block: %u, Total: %u"), maxBlock, totalFree);
    return false;
  }
  return true;
}

ApiClient::UploadRecordLoad ApiClient::loadRecordFromRtc(size_t& record_len) {
  record_len = 0;
  char* buf = sharedBuffer();
  const size_t buf_len = sharedBufferSize();
  if (!buf || buf_len == 0) {
    return UploadRecordLoad::FATAL;
  }

  RtcSensorRecord rec;
  const RtcReadStatus status = RtcManager::peekEx(rec);
  if (status == RtcReadStatus::CACHE_EMPTY) {
    return UploadRecordLoad::EMPTY;
  }
  if (status == RtcReadStatus::SCANNING) {
    return UploadRecordLoad::RETRY;
  }
  if (status == RtcReadStatus::CORRUPT_DATA) {
    if (m_loadedRecordSource == UploadRecordSource::RTC) {
      m_currentRecordSentToGateway = false;
      m_forceCloudAfterEdgeFailure = false;
      m_loadedRecordSource = UploadRecordSource::NONE;
    }
    broadcastEncrypted("[RTC] Corrupt slot dropped, retrying RTC read.");
    return UploadRecordLoad::RETRY;
  }
  if (status == RtcReadStatus::FILE_READ_ERROR) {
    return UploadRecordLoad::FATAL;
  }

  char timeBuf[20] = "1970-01-01 00:00:00";
  if (rec.timestamp > NTP_VALID_TIMESTAMP_THRESHOLD) {
    struct tm timeinfo;
    time_t t = static_cast<time_t>(rec.timestamp);
    localtime_r(&t, &timeinfo);
    format_datetime(timeBuf, sizeof(timeBuf), timeinfo);
  }

  record_len = buildSensorPayload(buf,
                                  buf_len,
                                  static_cast<uint32_t>(GH_ID),
                                  static_cast<uint32_t>(NODE_ID),
                                  rec.temp10,
                                  rec.hum10,
                                  static_cast<uint32_t>(rec.lux),
                                  static_cast<int32_t>(rec.rssi),
                                  timeBuf,
                                  19);
  if (record_len == 0) {
    return UploadRecordLoad::FATAL;
  }
  buf[record_len] = '\0';
  return UploadRecordLoad::READY;
}

ApiClient::UploadRecordLoad ApiClient::loadRecordFromLittleFs(size_t& record_len) {
  record_len = 0;
  char* buf = sharedBuffer();
  const size_t buf_len = sharedBufferSize();
  if (!buf || buf_len == 0) {
    return UploadRecordLoad::FATAL;
  }

  CacheReadError err = m_cacheManager.read_one(buf, buf_len - 1, record_len);
  if (err == CacheReadError::NONE && record_len > 0) {
    buf[record_len] = '\0';
    return UploadRecordLoad::READY;
  }
  if (err == CacheReadError::CACHE_EMPTY || (err == CacheReadError::NONE && record_len == 0)) {
    return UploadRecordLoad::EMPTY;
  }
  if (err == CacheReadError::SCANNING) {
    return UploadRecordLoad::RETRY;
  }
  if (err == CacheReadError::CORRUPT_DATA) {
    broadcastEncrypted("[SYSTEM] LittleFS record corrupt, dropped.");
    (void)m_cacheManager.pop_one();
    if (m_loadedRecordSource == UploadRecordSource::LITTLEFS) {
      m_currentRecordSentToGateway = false;
      m_forceCloudAfterEdgeFailure = false;
      m_loadedRecordSource = UploadRecordSource::NONE;
    }
    return UploadRecordLoad::RETRY;
  }
  return UploadRecordLoad::FATAL;
}

ApiClient::UploadRecordLoad ApiClient::loadRecordForUpload(size_t& record_len) {
  record_len = 0;

  if (m_loadedRecordSource == UploadRecordSource::RTC) {
    UploadRecordLoad locked = loadRecordFromRtc(record_len);
    if (locked == UploadRecordLoad::READY || locked == UploadRecordLoad::RETRY) {
      return locked;
    }
    m_loadedRecordSource = UploadRecordSource::NONE;
  } else if (m_loadedRecordSource == UploadRecordSource::LITTLEFS) {
    UploadRecordLoad locked = loadRecordFromLittleFs(record_len);
    if (locked == UploadRecordLoad::READY || locked == UploadRecordLoad::RETRY) {
      return locked;
    }
    m_loadedRecordSource = UploadRecordSource::NONE;
  }

  UploadRecordLoad rtcLoad = loadRecordFromRtc(record_len);
  if (rtcLoad == UploadRecordLoad::READY) {
    m_loadedRecordSource = UploadRecordSource::RTC;
    return UploadRecordLoad::READY;
  }

  UploadRecordLoad lfsLoad = loadRecordFromLittleFs(record_len);
  if (lfsLoad == UploadRecordLoad::READY) {
    m_loadedRecordSource = UploadRecordSource::LITTLEFS;
    return UploadRecordLoad::READY;
  }

  if (rtcLoad == UploadRecordLoad::FATAL || lfsLoad == UploadRecordLoad::FATAL) {
    return UploadRecordLoad::FATAL;
  }
  if (rtcLoad == UploadRecordLoad::RETRY || lfsLoad == UploadRecordLoad::RETRY) {
    return UploadRecordLoad::RETRY;
  }
  return UploadRecordLoad::EMPTY;
}

bool ApiClient::popLoadedRecord() {
  if (m_loadedRecordSource == UploadRecordSource::LITTLEFS) {
    for (uint8_t i = 0; i < 3; ++i) {
      if (m_cacheManager.pop_one()) {
        return true;
      }
      ESP.wdtFeed();
      yield();
    }
    return false;
  }
  if (m_loadedRecordSource == UploadRecordSource::RTC) {
    RtcSensorRecord discarded;
    for (uint8_t i = 0; i < 3; ++i) {
      RtcReadStatus status = RtcManager::popEx(discarded);
      if (status == RtcReadStatus::NONE || status == RtcReadStatus::CACHE_EMPTY) {
        return true;
      }
      if (status == RtcReadStatus::SCANNING || status == RtcReadStatus::CORRUPT_DATA) {
        continue;
      }
      return false;
    }
  }
  return false;
}

void ApiClient::applyQueuePopFailureCooldown(const AppConfig& cfg, const char* sourceTag) {
  if (m_queuePopFailStreak < 8) {
    m_queuePopFailStreak++;
  }
  const uint8_t shift = static_cast<uint8_t>(std::min<uint8_t>(m_queuePopFailStreak, 6));
  unsigned long cooldownMs = (1000UL << shift);
  cooldownMs = std::min<unsigned long>(cooldownMs, 60000UL);
  m_queuePopRetryAfter = millis() + cooldownMs;

  const unsigned long nextInterval = std::max<unsigned long>(cfg.CACHE_SEND_INTERVAL_MS, cooldownMs);
  m_cacheSendTimer.setInterval(nextInterval);
  m_cacheSendTimer.reset();

  char msg[144];
  int n = snprintf(msg,
                   sizeof(msg),
                   "[QUEUE] Pop %s failed, cooldown %lu ms (streak=%u)",
                   sourceTag ? sourceTag : "Unknown",
                   cooldownMs,
                   static_cast<unsigned>(m_queuePopFailStreak));
  if (n > 0) {
    const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
    broadcastEncrypted(std::string_view(msg, len));
  }
}

bool ApiClient::enqueueEmergencyRecord(const EmergencyRecord& record) {
  if (m_emergencyCount >= kEmergencyQueueCapacity) {
    return false;
  }
  m_emergencyQueue[m_emergencyHead] = record;
  m_emergencyHead = static_cast<uint8_t>((m_emergencyHead + 1U) % kEmergencyQueueCapacity);
  m_emergencyCount++;
  return true;
}

bool ApiClient::peekEmergencyRecord(EmergencyRecord& out) const {
  if (m_emergencyCount == 0) {
    return false;
  }
  out = m_emergencyQueue[m_emergencyTail];
  return true;
}

bool ApiClient::popEmergencyRecord(EmergencyRecord& out) {
  if (m_emergencyCount == 0) {
    return false;
  }
  out = m_emergencyQueue[m_emergencyTail];
  m_emergencyTail = static_cast<uint8_t>((m_emergencyTail + 1U) % kEmergencyQueueCapacity);
  m_emergencyCount--;
  if (m_emergencyCount == 0) {
    m_emergencyHead = 0;
    m_emergencyTail = 0;
  }
  return true;
}

void ApiClient::logEmergencyQueueState(const char* reason) {
  const unsigned long nowMs = millis();
  const bool transition = (reason &&
                           (strcmp(reason, "queue-full") == 0 ||
                            strcmp(reason, "backpressure-off") == 0 ||
                            strcmp(reason, "enqueue") == 0));
  if (!transition && (nowMs - m_lastEmergencyLogMs) < 5000UL) {
    return;
  }
  m_lastEmergencyLogMs = nowMs;

  LOG_WARN("EMERG",
           F("%s depth=%u/%u backpressure=%u rtc=%u lfs=%lu"),
           reason ? reason : "state",
           static_cast<unsigned>(m_emergencyCount),
           static_cast<unsigned>(kEmergencyQueueCapacity),
           static_cast<unsigned>(m_emergencyBackpressure ? 1 : 0),
           static_cast<unsigned>(RtcManager::getCount()),
           static_cast<unsigned long>(m_cacheManager.get_size()));

  char msg[168];
  int n = snprintf(msg,
                   sizeof(msg),
                   "[EMERG] %s | depth %u/%u | backpressure=%u | RTC %u/%u | LittleFS %lu/%lu B",
                   reason ? reason : "state",
                   static_cast<unsigned>(m_emergencyCount),
                   static_cast<unsigned>(kEmergencyQueueCapacity),
                   static_cast<unsigned>(m_emergencyBackpressure ? 1 : 0),
                   static_cast<unsigned>(RtcManager::getCount()),
                   static_cast<unsigned>(RTC_MAX_RECORDS),
                   static_cast<unsigned long>(m_cacheManager.get_size()),
                   static_cast<unsigned long>(MAX_CACHE_DATA_SIZE));
  if (n > 0) {
    const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
    broadcastEncrypted(std::string_view(msg, len));
  }
}

bool ApiClient::persistEmergencyRecord(const EmergencyRecord& record, bool allowDirectSend) {
  const unsigned long nowMs = millis();
  static constexpr uint8_t kRtcAppendRetries = 3;
  static constexpr uint8_t kFallbackFsWriteRetries = 2;
  static constexpr unsigned long kFallbackFsCooldownBaseMs = 5000UL;
  static constexpr unsigned long kFallbackFsCooldownMaxMs = 60000UL;

  for (uint8_t rtcAttempt = 1; rtcAttempt <= kRtcAppendRetries; ++rtcAttempt) {
    if (RtcManager::append(record.timestamp, record.temp10, record.hum10, record.lux, record.rssi)) {
      m_rtcFallbackFsFailStreak = 0;
      m_rtcFallbackFsRetryAfter = 0;
      if (allowDirectSend) {
        char msg[128];
        int n = snprintf(msg,
                         sizeof(msg),
                         "[CACHE] Stored in RTC | RTC %u/%u | LittleFS %lu/%lu B",
                         static_cast<unsigned>(RtcManager::getCount()),
                         static_cast<unsigned>(RTC_MAX_RECORDS),
                         static_cast<unsigned long>(m_cacheManager.get_size()),
                         static_cast<unsigned long>(MAX_CACHE_DATA_SIZE));
        if (n > 0) {
          const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
          broadcastEncrypted(std::string_view(msg, len));
        }
      }
      return true;
    }
    LOG_WARN("API", F("RTC append attempt %u/%u failed"),
             static_cast<unsigned>(rtcAttempt),
             static_cast<unsigned>(kRtcAppendRetries));
    ESP.wdtFeed();
  }

  if (nowMs < m_rtcFallbackFsRetryAfter) {
    const unsigned long waitMs = m_rtcFallbackFsRetryAfter - nowMs;
    LOG_WARN("API", F("RTC append failed and LittleFS fallback cooling down (%lu ms)"), waitMs);
    return false;
  }

  if (!ensureSharedBuffer()) {
    return false;
  }
  char* buf = sharedBuffer();
  const size_t buf_len = sharedBufferSize();
  if (!buf || buf_len == 0) {
    return false;
  }

  char timeBuf[20] = "1970-01-01 00:00:00";
  if (record.timestamp > NTP_VALID_TIMESTAMP_THRESHOLD) {
    tm timeinfo;
    time_t ts = static_cast<time_t>(record.timestamp);
    localtime_r(&ts, &timeinfo);
    format_datetime(timeBuf, sizeof(timeBuf), timeinfo);
  }

  const size_t payloadLen = buildSensorPayload(buf,
                                               buf_len,
                                               static_cast<uint32_t>(GH_ID),
                                               static_cast<uint32_t>(NODE_ID),
                                               static_cast<int32_t>(record.temp10),
                                               static_cast<int32_t>(record.hum10),
                                               static_cast<uint32_t>(record.lux),
                                               static_cast<int32_t>(record.rssi),
                                               timeBuf,
                                               19);
  if (payloadLen == 0) {
    LOG_ERROR("API", F("Fallback payload build failed"));
    return false;
  }

  for (uint8_t fsAttempt = 1; fsAttempt <= kFallbackFsWriteRetries; ++fsAttempt) {
    if (m_cacheManager.write(buf, payloadLen)) {
      m_rtcFallbackFsFailStreak = 0;
      m_rtcFallbackFsRetryAfter = 0;
      if (allowDirectSend) {
        LOG_WARN("API", F("RTC append failed, record stored directly to LittleFS fallback"));
        char msg[128];
        int n = snprintf(msg,
                         sizeof(msg),
                         "[CACHE] RTC write failed, stored in LittleFS | RTC %u/%u | LittleFS %lu/%lu B",
                         static_cast<unsigned>(RtcManager::getCount()),
                         static_cast<unsigned>(RTC_MAX_RECORDS),
                         static_cast<unsigned long>(m_cacheManager.get_size()),
                         static_cast<unsigned long>(MAX_CACHE_DATA_SIZE));
        if (n > 0) {
          const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
          broadcastEncrypted(std::string_view(msg, len));
        }
      }
      return true;
    }
    LOG_WARN("API", F("Fallback LittleFS write attempt %u/%u failed"),
             static_cast<unsigned>(fsAttempt),
             static_cast<unsigned>(kFallbackFsWriteRetries));
    ESP.wdtFeed();
  }

  UploadResult directResult = {HTTPC_ERROR_CONNECTION_FAILED, false, "Emergency send failed"};
  if (allowDirectSend) {
    bool directToEdge = false;
    if (m_uploadMode == UploadMode::EDGE) {
      directToEdge = true;
    } else if (m_uploadMode == UploadMode::AUTO) {
      directToEdge = (m_cachedGatewayMode == 1) || m_localGatewayMode;
    }

    if (directToEdge) {
      const size_t edgeLen = prepareEdgePayload(payloadLen);
      if (edgeLen > 0) {
        directResult = performLocalGatewayUpload(buf, edgeLen);
      } else {
        directResult.httpCode = HTTPC_ERROR_TOO_LESS_RAM;
        snprintf(directResult.message, sizeof(directResult.message), "Edge encrypt fail");
      }
    } else {
      directResult = performSingleUpload(buf, payloadLen, false);
    }
    m_httpClient.reset();

    if (directResult.success) {
      m_rtcFallbackFsFailStreak = 0;
      m_rtcFallbackFsRetryAfter = 0;
      m_lastApiSuccessMillis = millis();
      m_consecutiveUploadFailures = 0;
      LOG_WARN("API",
               F("RTC+LittleFS failed, emergency direct send succeeded via %s (HTTP %d)"),
               directToEdge ? "EDGE" : "CLOUD",
               directResult.httpCode);
      return true;
    }
  }

  if (m_rtcFallbackFsFailStreak < 8) {
    m_rtcFallbackFsFailStreak++;
  }
  unsigned long cooldown = kFallbackFsCooldownBaseMs << m_rtcFallbackFsFailStreak;
  cooldown = std::min<unsigned long>(cooldown, kFallbackFsCooldownMaxMs);
  m_rtcFallbackFsRetryAfter = nowMs + cooldown;
  LOG_ERROR("API",
            F("Persist failed (RTC/LittleFS%s). Retry in %lu ms (code=%d, msg=%s)"),
            allowDirectSend ? "/DirectSend" : "",
            cooldown,
            directResult.httpCode,
            directResult.message);
  return false;
}

void ApiClient::drainEmergencyQueueToStorage(uint8_t maxRecords) {
  if (maxRecords == 0 || m_emergencyCount == 0) {
    return;
  }

  uint8_t drained = 0;
  while (drained < maxRecords && m_emergencyCount > 0) {
    EmergencyRecord front{};
    if (!peekEmergencyRecord(front)) {
      break;
    }
    if (!persistEmergencyRecord(front, false)) {
      break;
    }
    EmergencyRecord dropped{};
    if (!popEmergencyRecord(dropped)) {
      break;
    }
    drained++;
    ESP.wdtFeed();
    yield();
  }

  if (drained > 0) {
    logEmergencyQueueState("drained");
  }
  if (m_emergencyBackpressure && m_emergencyCount < kEmergencyQueueCapacity) {
    m_emergencyBackpressure = false;
    logEmergencyQueueState("backpressure-off");
  }
}


bool ApiClient::createAndCachePayload() {
  if (!ensureSharedBuffer()) {
    return false;
  }
  char* buf = sharedBuffer();
  const size_t buf_len = sharedBufferSize();
  if (!buf || buf_len == 0)
    return false;

  // Drain emergency queue first so old records are persisted before new sampling.
  drainEmergencyQueueToStorage(2);
  if (m_emergencyCount >= kEmergencyQueueCapacity) {
    m_emergencyBackpressure = true;
    logEmergencyQueueState("backpressure-hold");
    m_rssiSum = 0;
    m_sampleCount = 0;
    return true;
  }

  const auto& cfg = m_configManager.getConfig();
  int32_t temp10 = 0;
  int32_t hum10 = 0;
  SensorReading t = m_sensorManager.getTemp();
  if (t.isValid) {
    temp10 = round_to_int((t.value + cfg.TEMP_OFFSET) * 10.0f);
  }

  SensorReading h = m_sensorManager.getHumidity();
  if (h.isValid) {
    hum10 = round_to_int((h.value + cfg.HUMIDITY_OFFSET) * 10.0f);
  }

  uint16_t luxVal = 0;
  SensorReading l = m_sensorManager.getLight();
  if (l.isValid)
    luxVal = (uint16_t)(l.value * cfg.LUX_SCALING_FACTOR);

  long rssiVal = (m_sampleCount > 0) ? (m_rssiSum / m_sampleCount) : WiFi.RSSI();

  time_t now = time(nullptr);
  uint32_t timestamp = (now > NTP_VALID_TIMESTAMP_THRESHOLD) ? static_cast<uint32_t>(now) : 0U;
  EmergencyRecord record{};
  record.timestamp = timestamp;
  record.temp10 = static_cast<int16_t>(temp10);
  record.hum10 = static_cast<int16_t>(hum10);
  record.lux = luxVal;
  record.rssi = static_cast<int16_t>(rssiVal);

  if (!persistEmergencyRecord(record, true)) {
    if (enqueueEmergencyRecord(record)) {
      m_emergencyBackpressure = (m_emergencyCount >= kEmergencyQueueCapacity);
      logEmergencyQueueState("enqueue");
    } else {
      m_emergencyBackpressure = true;
      LOG_ERROR("API", F("Emergency queue full, entering sampling backpressure"));
      logEmergencyQueueState("queue-full");
    }
  } else if (m_emergencyBackpressure && m_emergencyCount < kEmergencyQueueCapacity) {
    m_emergencyBackpressure = false;
    logEmergencyQueueState("backpressure-off");
  }

  // If RTC is completely full, we must bulk flush to LittleFS
  if (RtcManager::isFull()) {
    flushRtcToLittleFs();
  }

  m_rssiSum = 0;
  m_sampleCount = 0;

  return true;
}

// Bulk Serializer: RTC -> LittleFS
void ApiClient::flushRtcToLittleFs() {
  LOG_WARN("RTC", F("[FLUSH]RTC Cache full! Bulk flushing %u records to LittleFS..."), RtcManager::getCount());
  {
    char msg[140];
    int n = snprintf(msg,
                     sizeof(msg),
                     "[CACHE] RTC->LittleFS flush started | RTC %u/%u | LittleFS %lu/%lu B",
                     static_cast<unsigned>(RtcManager::getCount()),
                     static_cast<unsigned>(RTC_MAX_RECORDS),
                     static_cast<unsigned long>(m_cacheManager.get_size()),
                     static_cast<unsigned long>(MAX_CACHE_DATA_SIZE));
    if (n > 0) {
      const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
      broadcastEncrypted(std::string_view(msg, len));
    }
  }
  char* buf = sharedBuffer();
  const size_t buf_len = sharedBufferSize();
  if (!buf || buf_len == 0) return;

  const uint16_t maxAttempts = static_cast<uint16_t>(RTC_MAX_RECORDS * 4);
  uint16_t attempts = 0;
  while (attempts < maxAttempts) {
    attempts++;

    RtcSensorRecord rec;
    RtcReadStatus peekStatus = RtcManager::peekEx(rec);
    if (peekStatus == RtcReadStatus::CACHE_EMPTY) {
      LOG_INFO("RTC", F("[FLUSH]RTC empty, flush complete"));
      broadcastEncrypted("[CACHE] RTC->LittleFS flush complete (RTC empty).");
      return;
    }
    if (peekStatus == RtcReadStatus::SCANNING) {
      LOG_WARN("RTC", F("[FLUSH]RTC recovery scanning in progress, retrying"));
      ESP.wdtFeed();
      continue;
    }
    if (peekStatus == RtcReadStatus::CORRUPT_DATA) {
      LOG_WARN("RTC", F("[FLUSH]RTC corrected corrupt front slot, retrying"));
      ESP.wdtFeed();
      continue;
    }
    if (peekStatus == RtcReadStatus::FILE_READ_ERROR) {
      LOG_ERROR("RTC", F("[FLUSH]RTC read/write error while peeking"));
      return;
    }

    char timeBuf[20] = "1970-01-01 00:00:00";
    if (rec.timestamp > NTP_VALID_TIMESTAMP_THRESHOLD) {
      struct tm timeinfo;
      time_t t = static_cast<time_t>(rec.timestamp);
      localtime_r(&t, &timeinfo);
      format_datetime(timeBuf, sizeof(timeBuf), timeinfo);
    }

    size_t payloadLen = buildSensorPayload(buf,
                                           buf_len,
                                           static_cast<uint32_t>(GH_ID),
                                           static_cast<uint32_t>(NODE_ID),
                                           rec.temp10,
                                           rec.hum10,
                                           static_cast<uint32_t>(rec.lux),
                                           static_cast<int32_t>(rec.rssi),
                                           timeBuf,
                                           19);
    if (payloadLen > 0) {
      if (!m_cacheManager.write(buf, payloadLen)) {
        LOG_ERROR("API", F("LittleFS write failed during bulk flush!"));
        return;
      }
    }

    RtcSensorRecord discarded;
    RtcReadStatus popStatus = RtcManager::popEx(discarded);
    if (popStatus == RtcReadStatus::NONE) {
      ESP.wdtFeed();  // Keep watchdog happy during multiple slow writes.
      continue;
    }
    if (popStatus == RtcReadStatus::SCANNING) {
      LOG_WARN("RTC", F("[FLUSH]RTC pop deferred: recovery scanning"));
      ESP.wdtFeed();
      continue;
    }
    if (popStatus == RtcReadStatus::CORRUPT_DATA) {
      LOG_WARN("RTC", F("[FLUSH]RTC pop hit corrupted data, retrying"));
      ESP.wdtFeed();
      continue;
    }
    if (popStatus == RtcReadStatus::CACHE_EMPTY) {
      LOG_INFO("RTC", F("[FLUSH]RTC empty after write, flush complete"));
      broadcastEncrypted("[CACHE] RTC->LittleFS flush complete (drained).");
      return;
    }
    if (popStatus == RtcReadStatus::FILE_READ_ERROR) {
      LOG_ERROR("RTC", F("[FLUSH]RTC read/write error while popping"));
      return;
    }
  }

  LOG_ERROR("RTC", F("[FLUSH]Aborted by guard loop (attempts=%u)"), attempts);
}

// Class method implementation (no longer a static function)

void ApiClient::processGatewayResult(const UploadResult& res) {
  const char* sourceText =
      (m_loadedRecordSource == UploadRecordSource::RTC) ? "RTC"
      : (m_loadedRecordSource == UploadRecordSource::LITTLEFS) ? "LittleFS"
                                                                : "Unknown";
  if (res.success) {
    LOG_INFO("GATEWAY", F("Notified: %s"), res.message);
    // Mark that we've notified gateway for this record
    m_currentRecordSentToGateway = true;
    m_forceCloudAfterEdgeFailure = false;
    char msg[112];
    int n = snprintf(msg, sizeof(msg), "[GATEWAY] OK from %s (pending cloud sync)", sourceText);
    if (n > 0) {
      const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
      broadcastEncrypted(std::string_view(msg, len));
    }
  } else {
    char msg[112];
    int n = snprintf(msg, sizeof(msg), "[GATEWAY] Fail from %s - retry cloud", sourceText);
    if (n > 0) {
      const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
      broadcastEncrypted(std::string_view(msg, len));
    }
  }
  // Pause to allow cloud retry. Do not remove from cache yet.
  m_uploadState = UploadState::IDLE;

  // FIX: Track failure for Edge/Gateway too!
  if (!res.success) {
    m_currentRecordSentToGateway = false;
    if (m_uploadMode == UploadMode::EDGE) {
      m_forceCloudAfterEdgeFailure = true;
    }
    trackUploadFailure();
  } else {
    // If success, we reset counter in handleSuccessfulUpload/processGatewayResult?
    // Wait, processGatewayResult success doesn't call handleSuccessfulUpload.
    // We should reset counter on success here too.
    m_consecutiveUploadFailures = 0;
  }
}

// --- NEW: Centralized Mode Control ---

int ApiClient::checkGatewayMode() {
  if (!m_httpClient) {
    m_httpClient.reset(new (std::nothrow) HTTPClient());
    if (!m_httpClient) return -1;
  }
  m_httpClient->setReuse(false);
  m_httpClient->setTimeout(2000);

  char modeUrls[4][MAX_URL_LEN] = {{0}};
  const size_t modeUrlCount =
      build_gateway_url_candidates(modeUrls, 4, kGatewayModePath, kGatewayModeUrl);
  if (modeUrlCount == 0) {
    broadcastEncrypted("[MODE] Gateway poll failed (no URL)");
    return -1;
  }

  for (size_t i = 0; i < modeUrlCount; ++i) {
    const char* modeUrl = modeUrls[i];
    if (!modeUrl || modeUrl[0] == '\0') {
      continue;
    }
    {
      char msg[96];
      int n = snprintf(msg, sizeof(msg), "[MODE] Gateway poll url=%s", modeUrl);
      if (n > 0) {
        broadcastEncrypted(std::string_view(msg, static_cast<size_t>(n)));
      }
    }

    if (!m_httpClient->begin(m_plainClient, modeUrl)) {
      continue;
    }

    int httpCode = m_httpClient->GET();
    {
      char msg[64];
      int n = snprintf(msg, sizeof(msg), "[MODE] Gateway poll http=%d", httpCode);
      if (n > 0) {
        broadcastEncrypted(std::string_view(msg, static_cast<size_t>(n)));
      }
    }
    if (httpCode != 200) {
      m_httpClient->end();
      continue;
    }

    char payload[96] = {0};
    WiFiClient& stream = REDACTED
    int n = stream.readBytes(payload, sizeof(payload) - 1);
    if (n > 0) {
      payload[n] = '\0';
      // Simple JSON parse: {"mode": X}
      const char* p = payload;
      const char* end = payload + n;
      while (p + 4 < end) {
        if (p[0] == 'm' && p[1] == 'o' && p[2] == 'd' && p[3] == 'e') {
          const char* c = p + 4;
          while (c < end && *c != ':') ++c;
          if (c < end && *c == ':') {
            ++c;
            while (c < end && (*c == ' ' || *c == '\t')) ++c;
            int val = 0;
            bool neg = false;
            if (c < end && *c == '-') { neg = true; ++c; }
            while (c < end && *c >= '0' && *c <= '9') {
              val = (val * 10) + (*c - '0');
              ++c;
            }
            val = neg ? -val : val;
            LOG_INFO("MODE", F("Gateway poll: %d"), val);
            {
              char msg[64];
              int m = snprintf(msg, sizeof(msg), "[MODE] Gateway mode=%d", val);
              if (m > 0) {
                broadcastEncrypted(std::string_view(msg, static_cast<size_t>(m)));
              }
            }
            m_httpClient->end();
            return val;
          }
        }
        ++p;
      }
    }
    m_httpClient->end();
  }
  broadcastEncrypted("[MODE] Gateway poll failed");
  return -1;  // Fail
}

// =============================================================================
// Main Upload Cycle (Refactored)
// =============================================================================

// Helper to prepare Edge Payload (Encrypt)
// Returns length of prepared payload in m_sharedBuffer
// Injects edge-only metadata before encryption

size_t ApiClient::prepareEdgePayload(size_t rawLen) {
  if (!ensureSharedBuffer()) {
    return 0;
  }
  char* buf = sharedBuffer();
  const size_t buf_len = sharedBufferSize();
  if (!buf || buf_len == 0)
    return 0;
  char* closingBrace = strrchr(buf, '}');
  if (!closingBrace)
    return 0;

  char sendTimeValue[24] = "1970-01-01 00:00:00";
  size_t sendTimeLen = sizeof("1970-01-01 00:00:00") - 1;
  (void)extract_recorded_at_value(buf, sendTimeValue, sizeof(sendTimeValue), sendTimeLen);

  if (!strip_recorded_at_field(buf, rawLen)) {
    return 0;
  }
  closingBrace = strrchr(buf, '}');
  if (!closingBrace) {
    return 0;
  }

  const int32_t nonActiveRssi = static_cast<int32_t>(resolve_nonactive_rssi(m_wifiManager));

  char edgeOnlyFields[64];
  size_t fieldsPos = 0;
  static constexpr char kRssiNonActivePrefix[] = ",\"rssi_nonactive\":";
  static constexpr char kSendTimePrefix[] = ",\"send_time\":\"";

  if (!append_bytes_strict(edgeOnlyFields,
                           sizeof(edgeOnlyFields),
                           fieldsPos,
                           kRssiNonActivePrefix,
                           sizeof(kRssiNonActivePrefix) - 1)) {
    return 0;
  }
  if (!append_i32_strict(edgeOnlyFields, sizeof(edgeOnlyFields), fieldsPos, nonActiveRssi)) {
    return 0;
  }
  if (!append_bytes_strict(edgeOnlyFields,
                           sizeof(edgeOnlyFields),
                           fieldsPos,
                           kSendTimePrefix,
                           sizeof(kSendTimePrefix) - 1)) {
    return 0;
  }
  if (!append_bytes_strict(edgeOnlyFields, sizeof(edgeOnlyFields), fieldsPos, sendTimeValue, sendTimeLen)) {
    return 0;
  }
  if (!append_char_strict(edgeOnlyFields, sizeof(edgeOnlyFields), fieldsPos, '"')) {
    return 0;
  }

  if (!append_char_strict(edgeOnlyFields, sizeof(edgeOnlyFields), fieldsPos, '}')) {
    return 0;
  }

  const size_t insertPos = static_cast<size_t>(closingBrace - buf);
  if (insertPos + fieldsPos >= buf_len) {
    return 0;
  }
  memcpy(buf + insertPos, edgeOnlyFields, fieldsPos + 1);
  rawLen = insertPos + fieldsPos;

  std::array<char, CryptoUtils::ENCRYPTION_BUFFER_SIZE + 4> encBuffer{};
  strcpy(encBuffer.data(), "ENC:");

  size_t encLen = CryptoUtils::fast_serialize_encrypted_main(
      std::string_view(buf, rawLen), encBuffer.data() + 4, encBuffer.size() - 4);

  if (encLen == 0)
    return 0;

  size_t totalLen = REDACTED
  if (totalLen >= REDACTED
    return 0;

  memcpy(buf, encBuffer.data(), totalLen);
  buf[totalLen] = REDACTED
  return totalLen;
}


void ApiClient::handleUploadCycle() {
  // If state machine is running, let it run
  if (m_httpState != HttpState::IDLE)
    return;

  if (m_otaInProgress) {
    // Defer uploads while OTA is active
    m_cacheSendTimer.reset();
    return;
  }

  const bool ntpSynced = m_ntpClient.isTimeSynced();
  const AppConfig& cfg = m_configManager.getConfig();

  if (m_wifiManager.isScanBusy()) {
    // Avoid TLS/HTTP connects while WiFi is scanning to prevent OOM.
    m_cacheSendTimer.reset();
    return;
  }

  if (!isHeapHealthy()) {
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    uint32_t totalFree = REDACTED

    notifyLowMemory(maxBlock, totalFree);
    m_uploadState = UploadState::IDLE;
    // Retry usually happens on next timer tick, which is safer than immediate retry
    m_cacheSendTimer.reset();

    // FIX: Add Reboot Counter if Low Memory persistent
    m_lowMemCounter++;

    // If > 10 cycles (approx 2.5 mins if 15s interval) memory is still critical, reboot.
    if (m_lowMemCounter > 10) {
      LOG_ERROR("MEM", F("Critical Memory Fragmentation persistent. Rebooting to self-heal."));
      delay(1000);
      ESP.restart();
    }
    return;
  }

  // Reset counter if memory healthy
  m_lowMemCounter = 0;

  if (!ensureSharedBuffer()) {
    m_uploadState = UploadState::IDLE;
    return;
  }
  char* buf = sharedBuffer();
  const size_t buf_len = sharedBufferSize();
  if (!buf || buf_len == 0) {
    m_uploadState = UploadState::IDLE;
    return;
  }

  if (m_queuePopRetryAfter != 0 &&
      static_cast<int32_t>(millis() - m_queuePopRetryAfter) < 0) {
    m_uploadState = UploadState::IDLE;
    releaseSharedBuffer();
    return;
  }

  if (m_queuePopFailStreak > 0 && m_loadedRecordSource != UploadRecordSource::NONE) {
    const char* sourceText =
        (m_loadedRecordSource == UploadRecordSource::RTC) ? "RTC"
        : (m_loadedRecordSource == UploadRecordSource::LITTLEFS) ? "LittleFS"
                                                                  : "Unknown";
    if (popLoadedRecord()) {
      m_queuePopFailStreak = 0;
      m_queuePopRetryAfter = 0;
      m_loadedRecordSource = UploadRecordSource::NONE;
      m_cacheSendTimer.setInterval(cfg.CACHE_SEND_INTERVAL_MS);
      m_cacheSendTimer.reset();
      char msg[96];
      int n = snprintf(msg, sizeof(msg), "[QUEUE] Recovery OK: pop %s succeeded.", sourceText);
      if (n > 0) {
        const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
        broadcastEncrypted(std::string_view(msg, len));
      }
    } else {
      applyQueuePopFailureCooldown(cfg, sourceText);
    }
    m_uploadState = UploadState::IDLE;
    releaseSharedBuffer();
    return;
  }

  ESP.wdtFeed();
  size_t record_len = 0;
  UploadRecordLoad loadStatus = loadRecordForUpload(record_len);
  if (loadStatus == UploadRecordLoad::EMPTY) {
    m_currentRecordSentToGateway = false;
    m_forceCloudAfterEdgeFailure = false;
    m_loadedRecordSource = UploadRecordSource::NONE;
    m_uploadState = UploadState::IDLE;
    releaseSharedBuffer();
    return;
  }
  if (loadStatus == UploadRecordLoad::RETRY) {
    releaseSharedBuffer();
    return;
  }
  if (loadStatus == UploadRecordLoad::FATAL) {
    m_uploadState = UploadState::IDLE;
    m_cacheSendTimer.reset();
    releaseSharedBuffer();
    return;
  }

  buf[record_len] = '\0';

  // Determine Target Mode
  bool isTargetEdge = false;
  int gwMode = -1;

  // --- NEW: Centralized Control Logic (Poll before decide) ---
  // Only Poll if in AUTO mode (User override takes precedence)
  if (m_uploadMode == UploadMode::AUTO) {
    gwMode = m_cachedGatewayMode;
    const unsigned long nowMs = millis();
    if (gwMode < 0 || (nowMs - m_lastGatewayModeCheck) >= GATEWAY_MODE_TTL_MS) {
      gwMode = checkGatewayMode();
      m_cachedGatewayMode = static_cast<int8_t>(gwMode);
      m_lastGatewayModeCheck = nowMs;
    }
    // 0=Cloud, 1=Local, 2=Auto
    if (gwMode == 0) {
      // Gateway says: FORCE CLOUD
      isTargetEdge = false;
      // IMPORTANT: If we were in fallback gateway mode, clear it?
      // Spec says: "Admin sets mode cloud -> Node sends only to Cloud"
      if (m_localGatewayMode) {
        m_localGatewayMode = false;
        LOG_INFO("MODE", F("Gateway enforced CLOUD mode"));
      }
    } else if (gwMode == 1) {
      // Gateway says: FORCE LOCAL
      isTargetEdge = true;
      if (!m_localGatewayMode) {
        m_localGatewayMode = true;
        LOG_INFO("MODE", F("Gateway enforced LOCAL mode"));
      }
    } else {
      // Gateway says: AUTO (2) or Failed (-1) -> Use our standard logic
      // (Fall through to existing logic below)
    }
  }

  if (m_uploadMode == UploadMode::EDGE) {
    if (m_forceCloudAfterEdgeFailure) {
      LOG_WARN("UPLOAD", F("EDGE fallback: gateways unavailable, trying cloud..."));
      isTargetEdge = false;
      m_forceCloudAfterEdgeFailure = false;
    } else if (!m_currentRecordSentToGateway) {
      // Priority 1: Send to Gateway (Real-time monitoring)
      isTargetEdge = true;
    } else {
      // Priority 2: Sync to Cloud (Clear cache)
      // Use backoff to prevent flooding if internet is down
      if (millis() - m_lastCloudRetryAttempt >= AppConstants::CLOUD_RETRY_INTERVAL_MS) {
        LOG_INFO("UPLOAD", F("EDGE Mode: Syncing to cloud..."));
        m_lastCloudRetryAttempt = millis();
        isTargetEdge = false;
      } else {
        // Waiting for cloud retry - do nothing (keep data in cache)
        m_uploadState = UploadState::IDLE;
        releaseSharedBuffer();
        return;
      }
    }
  } else if (m_uploadMode == UploadMode::AUTO) {
    if (m_localGatewayMode) {
      isTargetEdge = true;

      // RESTORED RECOVERY LOGIC: Periodically retry Cloud even if in Gateway mode
      // BUT ONLY IF Gateway didn't explicitly force LOCAL (Mode 1).
      // If mode is explicitly LOCAL, keep data path pinned to gateway.

      if (gwMode != 1 &&
          millis() - m_lastCloudRetryAttempt >= AppConstants::CLOUD_RETRY_INTERVAL_MS) {
        LOG_INFO("UPLOAD", F("Auto Mode: Retrying cloud..."));
        m_lastCloudRetryAttempt = millis();
        isTargetEdge = false;  // Force Cloud attempt
      }
    }
  }

  if (!ntpSynced && !isTargetEdge) {
    static unsigned long lastNtpWarn = 0;
    if (millis() - lastNtpWarn > 60000UL) {
      LOG_WARN("TIME", F("NTP not synced; cloud upload deferred"));
      lastNtpWarn = millis();
    }
    m_uploadState = UploadState::IDLE;
    m_cacheSendTimer.reset();
    releaseSharedBuffer();
    return;
  }

  if (isTargetEdge) {
    // Prepare Encrypted Payload
    size_t encLen = prepareEdgePayload(record_len);
    if (encLen > 0) {
      const char* sourceText =
          (m_loadedRecordSource == UploadRecordSource::RTC) ? "RTC"
          : (m_loadedRecordSource == UploadRecordSource::LITTLEFS) ? "LittleFS"
                                                                    : "Unknown";
      char msg[96];
      int n = snprintf(msg, sizeof(msg), "[UPLOAD] EDGE send from %s (%u B)", sourceText, static_cast<unsigned>(record_len));
      if (n > 0) {
        const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
        broadcastEncrypted(std::string_view(msg, len));
      }
      startUpload(buf, encLen, true);
    } else {
      LOG_ERROR("API", F("Encryption failed. Skipping."));
    }
  } else {
    // CLOUD or AUTO (Cloud Safe)
    const char* sourceText =
        (m_loadedRecordSource == UploadRecordSource::RTC) ? "RTC"
        : (m_loadedRecordSource == UploadRecordSource::LITTLEFS) ? "LittleFS"
                                                                  : "Unknown";
    char msg[104];
    int n = snprintf(msg, sizeof(msg), "[UPLOAD] HTTPS send from %s (%u B)", sourceText, static_cast<unsigned>(record_len));
    if (n > 0) {
      const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
      broadcastEncrypted(std::string_view(msg, len));
    }
    startUpload(buf, record_len, false);
  }
}

