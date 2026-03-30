#include "api/ApiClient.TransportController.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecureBearSSL.h>
#include <bearssl/bearssl_hmac.h>
#include <user_interface.h>

#include <algorithm>
#include <cstring>
#include <strings.h>

#include "storage/CacheManager.h"
#include "support/CompileTimeJSON.h"
#include "system/ConfigManager.h"
#include "support/CryptoUtils.h"
#include "system/Logger.h"
#include "net/NtpClient.h"
#include "system/NodeIdentity.h"
#include "storage/RtcManager.h"
#include "sensor/SensorManager.h"
#include "REDACTED"
#include "config/constants.h"
#include "generated/node_config.h"
#include "support/Utils.h"

#include "api/ApiClient.Health.h"
#include "api/ApiClient.TransportShared.h"

using namespace ApiClientTransportShared;

// ApiClient.TransportSingle.cpp - blocking single-request HTTPS upload path

UploadResult ApiClientTransportController::performSingleUpload(const char* payload,
                                                              size_t length,
                                                              bool allowInsecure) {
  UploadResult result = {HTTPC_ERROR_CONNECTION_FAILED, false, {0}};
  copy_trunc_P(result.message, sizeof(result.message), PSTR("Connection Failed"));
  const bool startedOnRelay = shouldUseRelayForCloudUpload();

  LOG_DEBUG("API", F("--- START UPLOAD ---"));
  LOG_DEBUG("API", F("Route: %s"), startedOnRelay ? "relay" : "direct");
  LOG_DEBUG("API", F("URL: %s"), startedOnRelay ? DEFAULT_RELAY_DATA_URL : m_deps.configManager.getDataUploadUrl());
  LOG_DEBUG("API", F("Length: %u"), length);
  LOG_DEBUG("API", F("Payload Content:\n%s"), payload);
  LOG_DEBUG("API", F("----------------------------"));

  if (!acquireTlsResources(allowInsecure)) {
    copy_trunc_P(result.message, sizeof(result.message), PSTR("Low TLS heap"));
    m_deps.configManager.releaseStrings();
    releaseSharedBuffer();
    return result;
  }

  {
    const ApiClientHealth::HeapBudget budget = ApiClientHealth::captureTlsHeapBudget(m_ctx);
    if (!budget.healthy) {
      copy_trunc_P(result.message, sizeof(result.message), PSTR("Low TLS heap"));
      result.httpCode = HTTPC_ERROR_TOO_LESS_RAM;
      releaseTlsResources();
      m_deps.configManager.releaseStrings();
      releaseSharedBuffer();
      return result;
    }
  }

  updateCloudTargetCacheFor(startedOnRelay);
  if (startedOnRelay && m_runtime.route.uplinkMode == UplinkMode::AUTO && m_runtime.route.forceRelayNextCloudAttempt) {
    m_runtime.route.forceRelayNextCloudAttempt = false;
  }
  m_deps.configManager.releaseStrings();

  char fallbackHost[sizeof("example.com")];
  char fallbackPath[sizeof("/api/sensor")];
  copy_trunc_P(fallbackHost, sizeof(fallbackHost), PSTR("example.com"));
  copy_trunc_P(fallbackPath, sizeof(fallbackPath), PSTR("/api/sensor"));
  const char* host = (m_transport.cloudHost[0] != '\0') ? m_transport.cloudHost : fallbackHost;
  const char* path = (m_transport.cloudPath[0] != '\0') ? m_transport.cloudPath : fallbackPath;

  yield();
  LOG_INFO("API", F("TLS pre-connect heap: %u, blk: %u"), ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
  if (!m_deps.secureClient.connect(host, 443)) {
    copy_trunc_P(result.message, sizeof(result.message), PSTR("TLS connect failed"));
    releaseTlsResources();
    m_deps.configManager.releaseStrings();
    if (shouldFallbackToRelay(result)) {
      activateRelayFallback();
      return performSingleUpload(payload, length, allowInsecure);
    }
    releaseSharedBuffer();
    return result;
  }

  char authBuf[kBearerHeaderBufferLen];
  const bool hasAuthHeader = REDACTED
  char userAgent[NodeIdentity::kUserAgentBufferLen];
  NodeIdentity::buildUserAgent(userAgent, sizeof(userAgent));
  char deviceId[NodeIdentity::kDeviceIdBufferLen];
  NodeIdentity::buildDeviceId(deviceId, sizeof(deviceId));

  m_deps.secureClient.print(F("POST "));
  m_deps.secureClient.print(path);
  m_deps.secureClient.print(F(" HTTP/1.1\r\nHost: "));
  m_deps.secureClient.print(host);
  m_deps.secureClient.print(
      F("\r\nConnection: close\r\nAccept: application/json\r\nContent-Type: application/json\r\nUser-Agent: "));
  m_deps.secureClient.print(userAgent);
  m_deps.secureClient.print(F("\r\nX-Device-ID: "));
  m_deps.secureClient.print(deviceId);
  m_deps.secureClient.print(F("\r\n"));
  if (hasAuthHeader) {
    m_deps.secureClient.print(F("Authorization: REDACTED
    m_deps.secureClient.print(authBuf);
    m_deps.secureClient.print(F("\r\n"));
  }
  m_deps.secureClient.print(F("Content-Length: "));
  m_deps.secureClient.print(length);
  m_deps.secureClient.print(F("\r\n\r\n"));
  if (length > 0 && payload) {
    const StreamWriteResult writeResult =
        write_all(m_deps.secureClient, reinterpret_cast<const uint8_t*>(payload), length, m_policy.writeTimeoutMs);
    if (writeResult.written < length) {
      result.httpCode = writeResult.disconnected ? HTTPC_ERROR_CONNECTION_LOST : HTTPC_ERROR_SEND_PAYLOAD_FAILED;
      copy_trunc_P(result.message,
                   sizeof(result.message),
                   writeResult.timedOut ? PSTR("Write timeout")
                                        : writeResult.disconnected ? PSTR("Connection Lost")
                                                                   : PSTR("Short write"));
      m_deps.secureClient.stop();
      releaseTlsResources();
      m_deps.configManager.releaseStrings();
      if (shouldFallbackToRelay(result)) {
        activateRelayFallback();
        return performSingleUpload(payload, length, allowInsecure);
      }
      releaseSharedBuffer();
      return result;
    }
  }

  char line[128];
  if (!read_line(m_deps.secureClient, line, sizeof(line), m_policy.secureLineTimeoutMs)) {
    result.httpCode = (!m_deps.secureClient.connected() && !m_deps.secureClient.available())
                          ? HTTPC_ERROR_CONNECTION_LOST
                          : HTTPC_ERROR_READ_TIMEOUT;
    copy_trunc_P(result.message,
                 sizeof(result.message),
                 result.httpCode == HTTPC_ERROR_CONNECTION_LOST ? PSTR("Connection Lost")
                                                                : PSTR("No HTTP response"));
    m_deps.secureClient.stop();
    releaseTlsResources();
    m_deps.configManager.releaseStrings();
    if (shouldFallbackToRelay(result)) {
      activateRelayFallback();
      return performSingleUpload(payload, length, allowInsecure);
    }
    releaseSharedBuffer();
    return result;
  }

  result.httpCode = parse_status_code(line);
  result.success = (result.httpCode >= 200 && result.httpCode < 300);

  char dateBuf[64] = {0};
  char locationBuf[64] = {0};
  parse_response_headers(m_deps.secureClient, dateBuf, sizeof(dateBuf), locationBuf, sizeof(locationBuf));
  sync_time_from_http_date(m_deps.ntpClient, dateBuf);
  char bodyPreview[192] = {0};
  const size_t bodyLen =
      read_body_preview(m_deps.secureClient, bodyPreview, sizeof(bodyPreview), m_policy.previewTimeoutMs);
  const bool wafBlocked = (bodyLen > 0 && response_body_indicates_waf_block(bodyPreview));
  if (wafBlocked) {
    result.success = false;
    if (result.httpCode >= 200 && result.httpCode < 300) {
      result.httpCode = 403;
    }
    copy_trunc_P(result.message, sizeof(result.message), PSTR("Imunify360 blocked"));
  }

  if (result.success) {
    copy_trunc_P(result.message, sizeof(result.message), PSTR("OK"));
  } else if (!wafBlocked && result.httpCode >= 300 && result.httpCode < 400 && locationBuf[0] != '\0') {
    char display[24];
    copy_location_display(display, sizeof(display), locationBuf);
    result.message[0] = '\0';
    size_t pos = 0;
    pos = append_literal_P(result.message, sizeof(result.message), pos, PSTR("HTTP "));
    pos = append_i32(result.message, sizeof(result.message), pos, result.httpCode);
    pos = append_literal_P(result.message, sizeof(result.message), pos, PSTR("->"));
    pos = append_literal(result.message, sizeof(result.message), pos, display);
  } else if (!wafBlocked) {
    buildErrorMessageSimple(result);
  }
  m_deps.secureClient.stop();
  releaseTlsResources();
  m_deps.configManager.releaseStrings();
  if (shouldFallbackToRelay(result)) {
    activateRelayFallback();
    return performSingleUpload(payload, length, allowInsecure);
  }
  releaseSharedBuffer();

  return result;
}
