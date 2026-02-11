#include "ApiClient.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecureBearSSL.h>
#include <bearssl/bearssl_hmac.h>
#include <user_interface.h>

#include <algorithm>
#include <cstring>
#include <strings.h>

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

// ApiClient.Transport.cpp - HTTP transport and state machine

namespace {
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
  inline void build_bearer(char* out, size_t out_len, const char* token, size_t token_len) {
    if (!out || out_len == 0) {
      return;
    }
    static constexpr char kPrefix[] = "Bearer ";
    constexpr size_t kPrefixLen = sizeof(kPrefix) - 1;
    size_t n = (token_len < (out_len - 1 - kPrefixLen)) ? token_len : (out_len - 1 - kPrefixLen);
    memcpy(out, kPrefix, kPrefixLen);
    if (n > 0) {
      memcpy(out + kPrefixLen, token, n);
    }
    out[kPrefixLen + n] = '\0';
  }

  inline void copy_trunc(char* dst, size_t dst_len, const char* src, size_t src_len) {
    if (!dst || dst_len == 0) {
      return;
    }
    size_t n = (src_len < (dst_len - 1)) ? src_len : (dst_len - 1);
    if (n > 0) {
      memcpy(dst, src, n);
    }
    dst[n] = '\0';
  }

  inline void copy_trunc(char* dst, size_t dst_len, const char* src) {
    if (!dst || dst_len == 0 || !src) {
      return;
    }
    size_t n = strnlen(src, dst_len - 1);
    if (n > 0) {
      memcpy(dst, src, n);
    }
    dst[n] = '\0';
  }

  inline size_t append_literal(char* out, size_t out_len, size_t pos, const char* text);
  inline size_t append_i32(char* out, size_t out_len, size_t pos, int32_t value);

  void resolveCloudTarget(const char* url, char* host, size_t host_len, char* path, size_t path_len) {
    if (host && host_len > 0)
      host[0] = '\0';
    if (path && path_len > 0)
      path[0] = '\0';

    if (!url || url[0] == '\0') {
      if (host && host_len > 0) {
        copy_trunc(host, host_len, "example.com", sizeof("example.com") - 1);
      }
      if (path && path_len > 0) {
        copy_trunc(path, path_len, "/api/sensor", sizeof("/api/sensor") - 1);
      }
      return;
    }

    const char* p = url;
    if (strncmp(p, "https://", 8) == 0) {
      p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
      p += 7;
    }

    const char* slash = strchr(p, '/');
    if (host && host_len > 0) {
      size_t len = slash ? static_cast<size_t>(slash - p) : strnlen(p, host_len - 1);
      len = std::min(len, host_len - 1);
      if (len > 0) {
        copy_trunc(host, host_len, p, len);
      } else {
        copy_trunc(host, host_len, "example.com", sizeof("example.com") - 1);
      }
    }

    if (path && path_len > 0) {
      if (slash) {
        copy_trunc(path, path_len, slash);
      } else {
        copy_trunc(path, path_len, "/api/sensor", sizeof("/api/sensor") - 1);
      }
    }
  }

  bool read_line(WiFiClient& client, char* out, size_t out_len, unsigned long timeoutMs) {
    if (!out || out_len == 0)
      return false;
    size_t pos = 0;
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
      while (client.available()) {
        char c = static_cast<char>(client.read());
        if (c == '\r')
          continue;
        if (c == '\n') {
          out[pos] = '\0';
          return true;
        }
        if (pos + 1 < out_len) {
          out[pos++] = c;
        }
      }
      if (!client.connected() && !client.available()) {
        break;
      }
      yield();
    }
    out[pos] = '\0';
    return pos > 0;
  }

  int parse_status_code(const char* line) {
    if (!line)
      return -1;
    const char* p = strchr(line, ' ');
    if (!p)
      return -1;
    while (*p == ' ')
      ++p;
    int code = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
      code = (code * 10) + (*p - '0');
      ++p;
      ++digits;
      if (digits >= 3)
        break;
    }
    return (digits >= 3) ? code : -1;
  }

  const char* lookup_http_reason(int code) {
    static constexpr struct {
      int code;
      const char* reason;
    } httpErrors[] = {{301, "Moved Permanently"},
                      {302, "Redirect"},
                      {303, "See Other"},
                      {307, "Temp Redirect"},
                      {308, "Perm Redirect"},
                      {400, "Bad Request"},
                      {401, "REDACTED"},
                      {403, "Forbidden"},
                      {404, "Not Found"},
                      {419, "Session Expired"},
                      {422, "Unprocessable"},
                      {429, "Too Many Requests"},
                      {500, "Server Error"}};
    for (const auto& err : httpErrors) {
      if (code == err.code) {
        return err.reason;
      }
    }
    return "Error";
  }

  void buildErrorMessageSimple(UploadResult& result) {
    if (result.success) {
      copy_trunc(result.message, sizeof(result.message), "OK", 2);
      return;
    }
    if (result.httpCode < 0) {
      result.message[0] = '\0';
      size_t pos = 0;
      pos = append_literal(result.message, sizeof(result.message), pos, "HTTP error ");
      pos = append_i32(result.message, sizeof(result.message), pos, result.httpCode);
      return;
    }
    const char* reason = lookup_http_reason(result.httpCode);
    copy_trunc(result.message, sizeof(result.message), reason);
  }

  bool readHeaderValue(HTTPClient& http, const char* name, char* out, size_t out_len) {
    if (!out || out_len == 0)
      return false;
    out[0] = '\0';
    String headerValue = http.header(name);
    if (headerValue.length() == 0) {
      return false;
    }
    headerValue.toCharArray(out, out_len);
    return true;
  }

  inline size_t append_literal(char* out, size_t out_len, size_t pos, const char* text) {
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

  inline size_t u32_to_dec(char* out, size_t out_len, uint32_t value) {
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

  inline size_t append_u32(char* out, size_t out_len, size_t pos, uint32_t value) {
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

  inline size_t append_i32(char* out, size_t out_len, size_t pos, int32_t value) {
    if (value < 0) {
      pos = append_literal(out, out_len, pos, "-");
      value = -value;
    }
    return append_u32(out, out_len, pos, static_cast<uint32_t>(value));
  }
// Keep STR macros available for later use in this translation unit.
}  // namespace

void ApiClient::tryNtpFallbackProbe() {
  // Proactively probe HTTP server for time if NTP is unresponsive > 60s.
  if (millis() > 60000 && millis() - m_lastTimeProbe > 60000) {
    m_lastTimeProbe = millis();
    LOG_WARN("TIME", F("NTP stuck. Probing HTTP server for 'Date' header..."));

    (void)performSingleUpload("{}", 2, true);
  }
}


void ApiClient::setupHttpHeaders(HTTPClient& http, const AppConfig& cfg) {
  (void)cfg;
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader(F("Connection"), F("close"));
  http.addHeader(F("Accept"), F("application/json"));
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("User-Agent"),
                 F("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                   "Chrome/120.0.0.0 Safari/537.36"));

  char authBuffer[MAX_TOKEN_LEN + 20];
  const char* token = REDACTED
  const size_t token_len = REDACTED
  build_bearer(authBuffer, sizeof(authBuffer), token, token_len);
  http.addHeader(F("REDACTED"), authBuffer);
}

void ApiClient::updateCloudTargetCache() {
  const char* url = m_configManager.getDataUploadUrl();
  resolveCloudTarget(url, m_cloudHost, sizeof(m_cloudHost), m_cloudPath, sizeof(m_cloudPath));
  m_configManager.releaseStrings();
}


void ApiClient::extractTimeFromResponse(HTTPClient& http) {
  if (m_ntpClient.isTimeSynced())
    return;

  char dateBuf[64] = {0};
  if (readHeaderValue(http, "Date", dateBuf, sizeof(dateBuf))) {
    time_t serverTime = Utils::parse_http_date(dateBuf);
    if (serverTime > 0) {
      m_ntpClient.setManualTime(serverTime);
    }
  }
}


void ApiClient::buildErrorMessage(UploadResult& result, HTTPClient& http) {
  if (result.success) {
    copy_trunc(result.message, sizeof(result.message), "OK", 2);
    return;
  }

  if (result.httpCode < 0) {
    result.message[0] = '\0';
    size_t pos = 0;
    pos = append_literal(result.message, sizeof(result.message), pos, "HTTP error ");
    pos = append_i32(result.message, sizeof(result.message), pos, result.httpCode);
    return;
  }

  // Error code lookup table - includes redirects and common errors
  static constexpr struct {
    int code;
    const char* reason;
  } httpErrors[] = {{301, "Moved Permanently"},
                    {302, "Redirect"},
                    {303, "See Other"},
                    {307, "Temp Redirect"},
                    {308, "Perm Redirect"},
                    {400, "Bad Request"},
                    {401, "REDACTED"},
                    {403, "Forbidden"},
                    {404, "Not Found"},
                    {419, "Session Expired"},
                    {422, "Unprocessable"},
                    {429, "Too Many Requests"},
                    {500, "Server Error"}};

  const char* reason = "Error";
  for (const auto& err : httpErrors) {
    if (result.httpCode == err.code) {
      reason = err.reason;
      break;
    }
  }

  // For redirects, try to show the Location header
  if (result.httpCode >= 300 && result.httpCode < 400) {
    char locBuf[64] = {0};
    if (readHeaderValue(http, "Location", locBuf, sizeof(locBuf))) {
      char display[24];
      size_t len = strnlen(locBuf, sizeof(locBuf));
      if (len > 20) {
        memcpy(display, locBuf, 17);
        memcpy(display + 17, "...", 3);
        display[20] = '\0';
      } else {
        copy_trunc(display, sizeof(display), locBuf, len);
      }
      result.message[0] = '\0';
      size_t pos = 0;
      pos = append_literal(result.message, sizeof(result.message), pos, reason);
      pos = append_literal(result.message, sizeof(result.message), pos, "->");
      pos = append_literal(result.message, sizeof(result.message), pos, display);
      return;
    }
  }

  result.message[0] = '\0';
  size_t pos = 0;
  pos = append_literal(result.message, sizeof(result.message), pos, "HTTP ");
  pos = append_i32(result.message, sizeof(result.message), pos, result.httpCode);
  pos = append_literal(result.message, sizeof(result.message), pos, " (");
  pos = append_literal(result.message, sizeof(result.message), pos, reason);
  pos = append_literal(result.message, sizeof(result.message), pos, ")");
}

// =============================================================================
// Local Gateway Fallback Methods
// =============================================================================


void ApiClient::signPayload(const char* payload, size_t payload_len, char* signatureBuffer) {
  br_hmac_key_context kc;
  br_hmac_context ctx;

  const char* token = REDACTED
  const size_t token_len = REDACTED
  if (token_len =REDACTED
    if (signatureBuffer) {
      signatureBuffer[0] = '\0';
    }
    LOG_ERROR("REDACTED", F("REDACTED"));
    return;
  }
  if (!payload || payload_len == 0) {
    if (signatureBuffer) {
      signatureBuffer[0] = '\0';
    }
    LOG_ERROR("API", F("Payload empty; cannot sign"));
    return;
  }

  // Initialize HMAC with SHA256 using AUTH_TOKEN as key
  br_hmac_key_init(&kc, &br_sha256_vtable, token, token_len);
  br_hmac_init(&ctx, &kc, 0);

  // Process payload
  br_hmac_update(&ctx, payload, payload_len);

  // Output
  uint8_t digest[32];
  br_hmac_out(&ctx, digest);

  // Hex encode (no sprintf)
  static const char hex[] = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    signatureBuffer[i * 2] = hex[(digest[i] >> 4) & 0x0F];
    signatureBuffer[i * 2 + 1] = hex[digest[i] & 0x0F];
  }
  signatureBuffer[64] = '\0';
}

// Dead code (performLocalGatewayUpload) removed


bool ApiClient::executeQosSample(HTTPClient& http,
                                 const char* url,
                                 const char* method,
                                 const char* payload,
                                 const AppConfig& cfg,
                                 unsigned long& duration) {
  ESP.wdtFeed();
  yield();

  unsigned long startTick = millis();
  int httpCode = -1;

  if (!acquireTlsResources(cfg.ALLOW_INSECURE_HTTPS())) {
    duration = millis() - startTick;
    return false;
  }

  if (http.begin(m_secureClient, url)) {
    char authBuf[128];
    const char* token = REDACTED
    const size_t token_len = REDACTED
    build_bearer(authBuf, sizeof(authBuf), token, token_len);
    http.addHeader(F("REDACTED"), authBuf);
    http.addHeader(F("User-Agent"), F("ESP8266-Node/QoS"));

    if (strcmp(method, "POST") == 0) {
      http.addHeader(F("Content-Type"), F("application/json"));
      httpCode = http.POST(payload);
    } else {
      httpCode = http.GET();
    }
    http.end();
  }

  releaseTlsResources();
  duration = millis() - startTick;
  return (httpCode > 0);
}


void ApiClient::updateResult(int code, bool success, const char* msg) {
  m_lastResult.httpCode = code;
  m_lastResult.success = success;
  copy_trunc(m_lastResult.message, sizeof(m_lastResult.message), msg);
}


void ApiClient::transitionState(HttpState newState) {
  m_httpState = newState;
  m_stateEntryTime = millis();
}


void ApiClient::startUpload(const char* payload, size_t length, bool isEdgeTarget) {
  if (m_httpState != HttpState::IDLE) {
    LOG_WARN("API", F("Upload request ignored - Busy"));
    return;
  }

  m_payloadLen = length;
  m_targetIsEdge = isEdgeTarget;  // Store decision for State Machine
  // Payload is already in m_sharedBuffer

  transitionState(HttpState::CONNECTING);
}

// FIXED: Removed stack-based host pointer logic flaw.
// FIXED: Cleaned up messy connection logic.
// =============================================================================
// State Machine Handlers
// =============================================================================


void ApiClient::handleStateConnecting(const AppConfig& cfg) {
  bool isEdge = m_targetIsEdge;

  const char* host = nullptr;
  uint16_t port = 0;

  if (isEdge) {
    static constexpr char kGatewayHost[] = "gateway-gh-" STR(GH_ID) ".local";
    host = kGatewayHost;
    port = 80;
    m_activeClient = &m_plainClient;
  } else {
    if (m_cloudHost[0] == '\0') {
      updateCloudTargetCache();
    }
    host = m_cloudHost;
    port = 443;
    m_activeClient = &m_secureClient;
  }

  broadcastUploadTarget(isEdge);

  if (!isEdge) {
    if (!acquireTlsResources(cfg.ALLOW_INSECURE_HTTPS())) {
      updateResult(HTTPC_ERROR_TOO_LESS_RAM, false, "Low TLS heap");
      transitionState(HttpState::FAILED);
      return;
    }
    const uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    const uint32_t totalFree = REDACTED
    uint32_t minBlock = AppConstants::TLS_MIN_SAFE_BLOCK_SIZE;
    uint32_t minTotal = REDACTED
    if (m_ws.count() > 0) {
      minBlock += 512;
      minTotal += REDACTED
    }
    if (maxBlock < minBlock || totalFree < minTotal) {
      updateResult(HTTPC_ERROR_TOO_LESS_RAM, false, "Low TLS heap");
      transitionState(HttpState::FAILED);
      return;
    }
  }
  m_activeClient->setTimeout(5000);
  // Connect
  if (m_activeClient->connect(host, port)) {
    transitionState(HttpState::SENDING_REQUEST);
  } else {
    updateResult(HTTPC_ERROR_CONNECTION_FAILED, false, "Connect Failed");
    transitionState(HttpState::FAILED);
  }
}


void ApiClient::handleStateSending(const AppConfig& cfg) {
  if (!m_activeClient || !m_activeClient->connected()) {
    updateResult(HTTPC_ERROR_CONNECTION_LOST, false, "Disconnected");
    transitionState(HttpState::FAILED);
    return;
  }
  char* buf = sharedBuffer();
  if (!buf) {
    updateResult(HTTPC_ERROR_CONNECTION_LOST, false, "No payload buffer");
    transitionState(HttpState::FAILED);
    return;
  }

  // Construct Headers - STREAMING MODE (No String/Heap allocation)
  if (!m_targetIsEdge && (m_cloudHost[0] == '\0' || m_cloudPath[0] == '\0')) {
    updateCloudTargetCache();
  }

  const char* path = m_targetIsEdge ? "/api/data" : m_cloudPath;
  const char* host = m_targetIsEdge ? "gateway.local" : m_cloudHost;

  // 1) Request line + Host + Common headers (grouped to reduce print calls)
  m_activeClient->print(F("POST "));
  m_activeClient->print(path);
  m_activeClient->print(F(" HTTP/1.1\r\nHost: "));
  m_activeClient->print(host);
  m_activeClient->print(
      F("\r\nConnection: close\r\n"
        "Content-Type: application/json\r\n"
        "Accept: application/json\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) ESP8266/Node\r\n"
        "Content-Length: "));
  m_activeClient->print(m_payloadLen);
  m_activeClient->print(F("\r\n"));

  // 4. Mode-Specific Headers
  if (m_targetIsEdge) {
    m_activeClient->print(F("X-Node-ID: "));
    m_activeClient->print(NODE_ID);
    m_activeClient->print(F("\r\n"));

    m_activeClient->print(F("X-GH-ID: "));
    m_activeClient->print(GH_ID);
    m_activeClient->print(F("\r\n"));

    char signature[65];
    signPayload(buf, m_payloadLen, signature);
    m_activeClient->print(F("X-Signature: "));
    m_activeClient->print(signature);
    m_activeClient->print(F("\r\n"));

    m_activeClient->print(F("X-Timestamp: "));
    m_activeClient->print((unsigned long)time(nullptr));
    m_activeClient->print(F("\r\n"));
  } else {
    m_activeClient->print(F("Authorization: REDACTED
    const char* token = REDACTED
    const size_t token_len = REDACTED
    m_activeClient->write(reinterpret_cast<const uint8_t*>(token), token_len);
    m_activeClient->print(F("\r\n"));
  }

  m_activeClient->print(F("\r\n"));  // End of headers

  // Write Body
  m_activeClient->write((const uint8_t*)buf, m_payloadLen);
  releaseSharedBuffer();

  transitionState(HttpState::WAITING_RESPONSE);
}


void ApiClient::handleStateWaiting(unsigned long stateDuration) {
  if (!m_activeClient) {
    updateResult(HTTPC_ERROR_CONNECTION_LOST, false, "Connection Lost");
    transitionState(HttpState::FAILED);
    return;
  }

  if (m_activeClient->available()) {
    transitionState(HttpState::READING_RESPONSE);
  } else {
    // When server closes after sending data, connected() can be false
    // while data is still pending in the buffer. Only treat as lost
    // after confirming no data is available.
    if (!m_activeClient->connected()) {
      updateResult(HTTPC_ERROR_CONNECTION_LOST, false, "Connection Lost");
      transitionState(HttpState::FAILED);
      return;
    }
    if (stateDuration > 10000) {  // 10s timeout
      updateResult(HTTPC_ERROR_READ_TIMEOUT, false, "Timeout");
      transitionState(HttpState::FAILED);
    }
  }
}


void ApiClient::handleStateReading() {
  // Parse Status Line
  char line[128];
  size_t n = m_activeClient->readBytesUntil('\n', line, sizeof(line) - 1);
  line[n] = '\0';
  Utils::trim_inplace(std::span<char>(line));

  const char* p = strchr(line, ' ');
  if (!p) {
    updateResult(-1, false, "Bad Response");
  } else {
    while (*p == ' ')
      ++p;
    if (*p < '0' || *p > '9') {
      updateResult(-1, false, "Bad Response");
    } else {
      int code = 0;
      while (*p >= '0' && *p <= '9') {
        code = (code * 10) + (*p - '0');
        ++p;
      }
      m_lastResult.httpCode = code;
      m_lastResult.success = (code >= 200 && code < 300);

      if (m_lastResult.success) {
        copy_trunc(m_lastResult.message, sizeof(m_lastResult.message), "OK", 2);
      } else {
        char location[64] = {0};
        if (code >= 300 && code < 400) {
          while (m_activeClient->available()) {
            char header[128];
            size_t hn = m_activeClient->readBytesUntil('\n', header, sizeof(header) - 1);
            header[hn] = '\0';
            Utils::trim_inplace(std::span<char>(header));
            if (header[0] == '\0')
              break;
            if (strncmp(header, "Location:", 9) == 0 || strncmp(header, "location:", 9) == 0) {
              const char* value = header + 9;
              while (*value == ' ')
                ++value;
              copy_trunc(location, sizeof(location), value);
              LOG_WARN("API", F("Redirect to: %s"), location);
              break;
            }
          }
        }

        if (location[0] != '\0') {
          char display[24];
          size_t len = strnlen(location, sizeof(display));
          if (len > 20) {
            memcpy(display, location, 17);
            memcpy(display + 17, "...", 3);
            display[20] = '\0';
          } else {
            copy_trunc(display, sizeof(display), location, len);
          }
          m_lastResult.message[0] = '\0';
          size_t pos = 0;
          pos = append_literal(m_lastResult.message, sizeof(m_lastResult.message), pos, "HTTP ");
          pos = append_i32(m_lastResult.message, sizeof(m_lastResult.message), pos, code);
          pos = append_literal(m_lastResult.message, sizeof(m_lastResult.message), pos, "->");
          pos = append_literal(m_lastResult.message, sizeof(m_lastResult.message), pos, display);
        } else {
          m_lastResult.message[0] = '\0';
          size_t pos = 0;
          pos = append_literal(m_lastResult.message, sizeof(m_lastResult.message), pos, "HTTP ");
          pos = append_i32(m_lastResult.message, sizeof(m_lastResult.message), pos, code);
        }
      }
    }
  }

  m_activeClient->stop();
  transitionState(HttpState::COMPLETE);
}


void ApiClient::handleUploadStateMachine() {
  const auto& cfg = m_configManager.getConfig();
  unsigned long stateDuration = millis() - m_stateEntryTime;

  switch (m_httpState) {
    case HttpState::IDLE:
      return;
    case HttpState::CONNECTING:
      handleStateConnecting(cfg);
      break;
    case HttpState::SENDING_REQUEST:
      handleStateSending(cfg);
      break;
    case HttpState::WAITING_RESPONSE:
      handleStateWaiting(stateDuration);
      break;
    case HttpState::READING_RESPONSE:
      handleStateReading();
      break;
    default:
      transitionState(HttpState::IDLE);
      break;
  }
}

// =============================================================================
// Main Handle Loop
// =============================================================================


UploadResult ApiClient::performImmediateUpload() {
  UploadResult result = {-1, false, "No data"};

  if (m_wifiManager.isScanBusy()) {
    copy_trunc(result.message, sizeof(result.message), "REDACTED", sizeof("REDACTED") - 1);
    return result;
  }

  // Create a fresh payload
  if (!createAndCachePayload()) {
    copy_trunc(result.message, sizeof(result.message), "Payload creation failed", sizeof("Payload creation failed") - 1);
    return result;
  }
  char* buf = sharedBuffer();
  const size_t buf_len = sharedBufferSize();
  if (!buf || buf_len == 0) {
    copy_trunc(result.message, sizeof(result.message), "No payload buffer", sizeof("No payload buffer") - 1);
    return result;
  }

  // Read the latest record from cache
  size_t record_len = 0;
  CacheReadError readErr = m_cacheManager.read_one(buf, buf_len, record_len);
  if (readErr != CacheReadError::NONE || record_len == 0) {
    if (readErr == CacheReadError::SCANNING) {
      copy_trunc(result.message, sizeof(result.message), "Cache scanning", sizeof("Cache scanning") - 1);
      return result;
    }
    if (readErr == CacheReadError::CORRUPT_DATA) {
      (void)m_cacheManager.pop_one();  // FORCE ADVANCE TAIL to prevent infinite loop
      copy_trunc(result.message, sizeof(result.message), "Cache corrupt - popped", sizeof("Cache corrupt - popped") - 1);
      broadcastEncrypted("[SYSTEM] Corrupt record cleared from cache.");
    } else {
      copy_trunc(result.message, sizeof(result.message), "Cache read failed", sizeof("Cache read failed") - 1);
    }
    return result;
  }

  LOG_INFO("API", F("Immediate upload: %u bytes"), record_len);

  // --- LOGIC REPLICATION FROM handleUploadCycle ---
  bool isTargetEdge = false;
  int gwMode = -1;
  if (m_uploadMode == UploadMode::AUTO) {
    if (m_immediatePollReady) {
      gwMode = m_immediateGatewayMode;
    } else {
      const uint32_t maxBlock = ESP.getMaxFreeBlockSize();
      const uint32_t totalFree = REDACTED
      if (maxBlock >= AppConstants::API_MIN_SAFE_BLOCK_SIZE && totalFree >= AppConstants::API_MIN_TOTAL_HEAP) {
        gwMode = checkGatewayMode();
        LOG_INFO("MODE", F("Immediate gateway poll: %d"), gwMode);
      } else {
        LOG_WARN("MODE",
                 F("Immediate gateway poll skipped (low heap: %u, block %u)"),
                 totalFree,
                 maxBlock);
      }
      m_immediateGatewayMode = static_cast<int8_t>(gwMode);
      m_immediatePollReady = true;
      // Defer cloud upload to the next loop to allow buffers to be freed.
      if (gwMode != 1) {
        m_immediateWarmup = 1;
        m_immediateUploadRequested = true;
        result.httpCode = kImmediateDeferred;
        result.success = false;
        copy_trunc(result.message, sizeof(result.message), "Deferred", sizeof("Deferred") - 1);
        return result;
      }
    }
  }

  // Poll Gateway if AUTO
  if (m_uploadMode == UploadMode::AUTO) {
    if (gwMode == 1) {  // 1 = Local
      isTargetEdge = true;
    }
    // If 0 (Cloud) or 2 (Auto) or -1 (Fail), default to Cloud (isTargetEdge = false)
  } else if (m_uploadMode == UploadMode::EDGE) {
    isTargetEdge = true;
  }

  broadcastUploadTarget(isTargetEdge);

  if (isTargetEdge) {
    // Edge logic: Encrypt and send to Gateway
    size_t encLen = prepareEdgePayload(record_len);
    if (encLen > 0) {
      // Temporarily point member buffer to prepared payload
      // Note: `performLocalGatewayUpload` logic was deleted earlier,
      // we must invoke a single POST to gateway IP.

      char url[64];
      buildLocalGatewayUrl(url, sizeof(url));

      m_httpClient.setReuse(false);
      m_httpClient.begin(m_plainClient, url);  // Use Plain Client
      // ... Headers ...
      m_httpClient.addHeader("Content-Type", "application/json");
      // Add custom headers manually since performSingleUpload is Cloud-specific?
      // Actually, let's reuse a generic uploader or duplicate just the POST part for robustness.
      // For immediate simplicity:

      int code = m_httpClient.POST((uint8_t*)buf, encLen);
      result.httpCode = code;
      result.success = (code >= 200 && code < 300);
      copy_trunc(result.message,
                 sizeof(result.message),
                 result.success ? "OK (Edge)" : "Fail (Edge)");
      m_httpClient.end();
    } else {
      result.success = false;
      copy_trunc(result.message, sizeof(result.message), "Encryption failed", sizeof("Encryption failed") - 1);
    }
  } else {
    // Cloud logic (Default)
    result = performSingleUpload(buf, record_len, false);
    if (result.httpCode == HTTPC_ERROR_TOO_LESS_RAM) {
      m_immediateWarmup = 1;
      m_immediateUploadRequested = true;
      result.httpCode = kImmediateDeferred;
      result.success = false;
      copy_trunc(result.message, sizeof(result.message), "Deferred", sizeof("Deferred") - 1);
      return result;
    }
  }

  // Handle result
  if (result.success) {
    (void)m_cacheManager.pop_one();  // Discard return - we just successfully sent it
    m_lastApiSuccessMillis = millis();
    m_consecutiveUploadFailures = 0;
  } else {
    m_consecutiveUploadFailures++;
  }
  m_immediatePollReady = false;
  m_immediateGatewayMode = -2;

  return result;
}


UploadResult ApiClient::performSingleUpload(const char* payload, size_t length, bool allowInsecure) {
  UploadResult result = {HTTPC_ERROR_CONNECTION_FAILED, false, "Connection Failed"};

  LOG_DEBUG("API", F("--- START UPLOAD ---"));
  LOG_DEBUG("API", F("URL: %s"), m_configManager.getDataUploadUrl());
  LOG_DEBUG("API", F("Length: %u"), length);
  LOG_DEBUG("API", F("Payload Content:\n%s"), payload);
  LOG_DEBUG("API", F("----------------------------"));

  if (!acquireTlsResources(allowInsecure)) {
    copy_trunc(result.message, sizeof(result.message), "Low TLS heap", sizeof("Low TLS heap") - 1);
    m_configManager.releaseStrings();
    return result;
  }

  {
    const uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    const uint32_t totalFree = REDACTED
    uint32_t minBlock = AppConstants::TLS_MIN_SAFE_BLOCK_SIZE;
    uint32_t minTotal = REDACTED
    if (m_ws.count() > 0) {
      minBlock += 512;
      minTotal += REDACTED
    }
    if (maxBlock < minBlock || totalFree < minTotal) {
      copy_trunc(result.message, sizeof(result.message), "Low TLS heap", sizeof("Low TLS heap") - 1);
      result.httpCode = HTTPC_ERROR_TOO_LESS_RAM;
      releaseTlsResources();
      m_configManager.releaseStrings();
      return result;
    }
  }

  if (m_cloudHost[0] == '\0' || m_cloudPath[0] == '\0') {
    updateCloudTargetCache();
  }
  // Free config strings before TLS handshake to maximize available heap.
  m_configManager.releaseStrings();

  const char* host = (m_cloudHost[0] != '\0') ? m_cloudHost : "example.com";
  const char* path = (m_cloudPath[0] != '\0') ? m_cloudPath : "/api/sensor";

  if (!m_secureClient.connect(host, 443)) {
    copy_trunc(result.message, sizeof(result.message), "TLS connect failed", sizeof("TLS connect failed") - 1);
    releaseTlsResources();
    m_configManager.releaseStrings();
    return result;
  }

  char authBuf[MAX_TOKEN_LEN + 20];
  const char* token = REDACTED
  const size_t token_len = REDACTED
  build_bearer(authBuf, sizeof(authBuf), token, token_len);

  m_secureClient.print(F("POST "));
  m_secureClient.print(path);
  m_secureClient.print(F(" HTTP/1.1\r\nHost: "));
  m_secureClient.print(host);
  m_secureClient.print(F("\r\nConnection: close\r\nAccept: application/json\r\nContent-Type: application/json\r\nUser-Agent: ESP8266-Node\r\nAuthorization: "));
  m_secureClient.print(authBuf);
  m_secureClient.print(F("\r\nContent-Length: "));
  m_secureClient.print(length);
  m_secureClient.print(F("\r\n\r\n"));
  if (length > 0 && payload) {
    m_secureClient.write(reinterpret_cast<const uint8_t*>(payload), length);
  }
  releaseSharedBuffer();

  char line[128];
  if (!read_line(m_secureClient, line, sizeof(line), 5000)) {
    copy_trunc(result.message, sizeof(result.message), "No HTTP response", sizeof("No HTTP response") - 1);
    m_secureClient.stop();
    releaseTlsResources();
    m_configManager.releaseStrings();
    return result;
  }

  result.httpCode = parse_status_code(line);
  result.success = (result.httpCode >= 200 && result.httpCode < 300);

  // Parse headers (Date only)
  char dateBuf[64] = {0};
  while (read_line(m_secureClient, line, sizeof(line), 5000)) {
    if (line[0] == '\0') {
      break;
    }
    if (strncasecmp(line, "Date:", 5) == 0) {
      const char* p = line + 5;
      while (*p == ' ' || *p == '\t')
        ++p;
      copy_trunc(dateBuf, sizeof(dateBuf), p);
    }
  }

  if (dateBuf[0] != '\0' && !m_ntpClient.isTimeSynced()) {
    time_t serverTime = Utils::parse_http_date(dateBuf);
    if (serverTime > 0) {
      m_ntpClient.setManualTime(serverTime);
    }
  }

  buildErrorMessageSimple(result);
  m_secureClient.stop();
  releaseTlsResources();
  m_configManager.releaseStrings();

  return result;
}

// =============================================================================
// handleUploadCycle Helper Methods (Reduce Cyclomatic Complexity)
// =============================================================================

// FIX: Gateway is just a notification - DON'T pop cache!
// Data must reach cloud before removal from cache
