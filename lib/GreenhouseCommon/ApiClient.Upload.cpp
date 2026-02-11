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

// ApiClient.Upload.cpp - payload creation and upload policy

namespace {
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
  static constexpr char kGatewayDataUrl[] = "http://gateway-gh-" STR(GH_ID) ".local/api/data";
  static constexpr char kGatewayModeUrl[] = "http://gateway-gh-" STR(GH_ID) ".local/api/mode";
  static constexpr char kGatewayDataPath[] = "/api/data";
  static constexpr char kGatewayModePath[] = "/api/mode";
  static constexpr char kEpochTimeStr[] = "1970-01-01 00:00:00";
  static constexpr size_t kDateTimeLen = 19;
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

  static bool is_valid_ipv4(const IPAddress& ip) {
    return (ip[0] | ip[1] | ip[2] | ip[3]) != 0;
  }

  static bool append_http_prefix(char* out, size_t out_len, size_t& pos) {
    static constexpr char kHttpPrefix[] = "http://";
    return append_bytes_strict(out, out_len, pos, kHttpPrefix, sizeof(kHttpPrefix) - 1);
  }

  static bool build_gateway_url_from_ip(char* out, size_t out_len, const IPAddress& ip, const char* path) {
    if (!out || out_len == 0 || !path)
      return false;
    if (!is_valid_ipv4(ip))
      return false;
    size_t pos = 0;
    if (!append_http_prefix(out, out_len, pos))
      return false;
    for (uint8_t i = 0; i < 4; ++i) {
      if (!append_u32_strict(out, out_len, pos, static_cast<uint32_t>(ip[i])))
        return false;
      if (i < 3) {
        if (!append_char_strict(out, out_len, pos, '.'))
          return false;
      }
    }
    return append_bytes_strict(out, out_len, pos, path, strnlen(path, out_len - pos - 1));
  }

  static bool build_gateway_url_from_ip_str(char* out, size_t out_len, const char* ip, const char* path) {
    if (!out || out_len == 0 || !ip || !path)
      return false;
    if (ip[0] == '\0')
      return false;
    size_t pos = 0;
    if (!append_http_prefix(out, out_len, pos))
      return false;
    pos = append_cstr(out, out_len, pos, ip);
    if (pos == 0 || pos >= out_len)
      return false;
    return append_bytes_strict(out, out_len, pos, path, strnlen(path, out_len - pos - 1));
  }

  static bool build_gateway_url(char* out, size_t out_len, const char* path, const char* mdns_url) {
    if (!out || out_len == 0 || !path)
      return false;
    out[0] = '\0';
    if (DEFAULT_GATEWAY_IP[0] != '\0') {
      if (build_gateway_url_from_ip_str(out, out_len, DEFAULT_GATEWAY_IP, path))
        return true;
    }
    if (mdns_url) {
      size_t n = strnlen(mdns_url, out_len - 1);
      memcpy(out, mdns_url, n);
      out[n] = '\0';
      return n > 0;
    }
    IPAddress gw = WiFi.gatewayIP();
    if (build_gateway_url_from_ip(out, out_len, gw, path))
      return true;
    return false;
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
  LOG_INFO("UPLOAD", F("Success: HTTP %d (%s)"), res.httpCode, res.message);
  m_lastApiSuccessMillis = millis();
  m_swWdtTimer.reset();

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

  if (m_cacheManager.pop_one()) {
    char msg[80];
    msg[0] = '\0';
    size_t pos = 0;
    pos = append_literal(msg, sizeof(msg), pos, "[SYSTEM] Upload OK (HTTP ");
    pos = append_i32(msg, sizeof(msg), pos, res.httpCode);
    pos = append_literal(msg, sizeof(msg), pos, ")");
    if (pos > 0) {
      broadcastEncrypted(std::string_view(msg, pos));
    }
  } else {
    m_uploadState = UploadState::IDLE;
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


bool ApiClient::createAndCachePayload() {
  const auto& cfg = m_configManager.getConfig();
  if (!ensureSharedBuffer()) {
    return false;
  }
  char* buf = sharedBuffer();
  const size_t buf_len = sharedBufferSize();
  if (!buf || buf_len == 0)
    return false;

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

  const char* timePtr = kEpochTimeStr;
  size_t timeLen = sizeof(kEpochTimeStr) - 1;
  time_t now = time(nullptr);
  if (now > NTP_VALID_TIMESTAMP_THRESHOLD) {
    if (now != m_cachedTimeEpoch) {
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);
      format_datetime(m_cachedTimeStr, sizeof(m_cachedTimeStr), timeinfo);
      m_cachedTimeEpoch = now;
    }
    timePtr = m_cachedTimeStr;
    timeLen = kDateTimeLen;
  } else {
    m_cachedTimeEpoch = 0;
  }
  size_t payloadLen = buildSensorPayload(buf,
                                         buf_len,
                                         static_cast<uint32_t>(GH_ID),
                                         static_cast<uint32_t>(NODE_ID),
                                         temp10,
                                         hum10,
                                         static_cast<uint32_t>(luxVal),
                                         static_cast<int32_t>(rssiVal),
                                         timePtr,
                                         timeLen);
  if (payloadLen == 0) {
    LOG_ERROR("API", F("Payload truncated!"));
    return false;
  }

  // Track and report maximum payload length observed at runtime.
  static size_t maxPayloadLen = 0;
  if (payloadLen > maxPayloadLen) {
    maxPayloadLen = payloadLen;
    LOG_INFO("API", F("Payload len=%u (max=%u)"), payloadLen, maxPayloadLen);
  }

  m_rssiSum = 0;
  m_sampleCount = 0;

  return m_cacheManager.write(buf, payloadLen);
}

// Class method implementation (no longer a static function)

void ApiClient::processGatewayResult(const UploadResult& res) {
  if (res.success) {
    LOG_INFO("GATEWAY", F("Notified: %s"), res.message);
    // Mark that we've notified gateway for this record
    m_currentRecordSentToGateway = true;
    broadcastEncrypted("[GATEWAY] Data forwarded (pending cloud sync)");
    broadcastEncrypted("[GATEWAY] Fail - will retry cloud directly");
  }
  // Pause to allow cloud retry. Do not remove from cache yet.
  m_uploadState = UploadState::IDLE;

  // FIX: Track failure for Edge/Gateway too!
  if (!res.success) {
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
  m_httpClient.setReuse(false);   // One-off request
  m_httpClient.setTimeout(2000);  // CRITICAL: Short timeout (2s)

  char modeUrl[MAX_URL_LEN] = {0};
  if (!build_gateway_url(modeUrl, sizeof(modeUrl), kGatewayModePath, kGatewayModeUrl)) {
    broadcastEncrypted("[MODE] Gateway poll failed (no URL)");
    return -1;
  }
  {
    char msg[96];
    int n = snprintf(msg, sizeof(msg), "[MODE] Gateway poll url=%s", modeUrl);
    if (n > 0) {
      broadcastEncrypted(std::string_view(msg, static_cast<size_t>(n)));
    }
  }

  if (m_httpClient.begin(m_plainClient, modeUrl)) {
    int httpCode = m_httpClient.GET();
    {
      char msg[64];
      int n = snprintf(msg, sizeof(msg), "[MODE] Gateway poll http=%d", httpCode);
      if (n > 0) {
        broadcastEncrypted(std::string_view(msg, static_cast<size_t>(n)));
      }
    }
    if (httpCode == 200) {
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
              m_httpClient.end();
              return val;
            }
          }
          ++p;
        }
      }
      m_httpClient.end();
    } else {
      m_httpClient.end();
    }
  }
  broadcastEncrypted("[MODE] Gateway poll failed");
  return -1;  // Fail
}

// =============================================================================
// Main Upload Cycle (Refactored)
// =============================================================================

// Helper to prepare Edge Payload (Encrypt)
// Returns length of prepared payload in m_sharedBuffer
// Injects send_time into payload for QoS delay calculation

size_t ApiClient::prepareEdgePayload(size_t rawLen) {
  if (!ensureSharedBuffer()) {
    return 0;
  }
  char* buf = sharedBuffer();
  const size_t buf_len = sharedBufferSize();
  if (!buf || buf_len == 0)
    return 0;
  time_t sendTime = time(nullptr);
  char* closingBrace = strrchr(buf, '}');
  if (!closingBrace || sendTime < NTP_VALID_TIMESTAMP_THRESHOLD)
    return 0;

  char sendTimeField[32];
  static constexpr char kPrefix[] = ",\"send_time\":";
  constexpr size_t kPrefixLen = sizeof(kPrefix) - 1;
  if (sizeof(sendTimeField) <= kPrefixLen + 2)
    return 0;
  memcpy(sendTimeField, kPrefix, kPrefixLen);
  size_t pos = kPrefixLen;
  pos += u32_to_dec(sendTimeField + pos, sizeof(sendTimeField) - pos, static_cast<uint32_t>(sendTime));
  if (pos + 2 > sizeof(sendTimeField))
    return 0;
  sendTimeField[pos++] = '}';
  sendTimeField[pos] = '\0';
  int fieldLen = static_cast<int>(pos);
  if (fieldLen <= 0)
    return 0;

  size_t insertPos = closingBrace - buf;
  size_t newLen = insertPos + fieldLen;
  if (newLen >= buf_len)
    return 0;

  memcpy(buf + insertPos, sendTimeField, fieldLen + 1);
  rawLen = newLen;

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

  if (!m_ntpClient.isTimeSynced())
    return;

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

  if (m_cacheManager.get_size() == 0) {
    m_uploadState = UploadState::IDLE;
    releaseSharedBuffer();
    return;
  }

  ESP.wdtFeed();
  size_t record_len = 0;
  CacheReadError err = m_cacheManager.read_one(buf, buf_len - 1, record_len);

  if (err != CacheReadError::NONE) {
    if (err == CacheReadError::SCANNING) {
      // Scan budget hit; try again next loop without altering state.
      releaseSharedBuffer();
      return;
    }
    if (err == CacheReadError::CORRUPT_DATA) {
      broadcastEncrypted("[SYSTEM] Cache corrupt record discarded.");
      (void)m_cacheManager.pop_one();  // FORCE ADVANCE TAIL
      releaseSharedBuffer();
      return;
    }
    m_uploadState = UploadState::IDLE;
    // Don't hammer the filesystem if it's failing
    m_cacheSendTimer.reset();
    releaseSharedBuffer();
    return;
  }

  buf[record_len] = '\0';
  m_currentRecordSentToGateway = false;
  // Determine Target Mode
  bool isTargetEdge = false;

  // --- NEW: Centralized Control Logic (Poll before decide) ---
  // Only Poll if in AUTO mode (User override takes precedence)
  if (m_uploadMode == UploadMode::AUTO) {
    int gwMode = m_cachedGatewayMode;
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
    if (!m_currentRecordSentToGateway) {
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
      // BUT ONLY IF Gateway didn't explicitly force LOCAL (Mode 1)
      // Since ApiClient doesn't store the "reason" for m_localGatewayMode, we rely on the poll result above.
      // If poll returned 1, we set m_localGatewayMode=true.
      // This simple fallback logic is fine for "AUTO" behavior.

      if (millis() - m_lastCloudRetryAttempt >= AppConstants::CLOUD_RETRY_INTERVAL_MS) {
        LOG_INFO("UPLOAD", F("Auto Mode: Retrying cloud..."));
        m_lastCloudRetryAttempt = millis();
        isTargetEdge = false;  // Force Cloud attempt
      }
    }
  }

  if (isTargetEdge) {
    // Prepare Encrypted Payload
    size_t encLen = prepareEdgePayload(record_len);
    if (encLen > 0) {
      startUpload(buf, encLen, true);
    } else {
      LOG_ERROR("API", F("Encryption failed. Skipping."));
    }
  } else {
    // CLOUD or AUTO (Cloud Safe)
    startUpload(buf, record_len, false);
  }
}
