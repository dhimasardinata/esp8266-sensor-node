#include "api/ApiClient.TransportController.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
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

#include "api/ApiClient.Health.h"
#include "api/ApiClient.TransportShared.h"

using namespace ApiClientTransportShared;

void ApiClientTransportController::tryNtpFallbackProbe() {
  if (millis() > 60000 && millis() - m_runtime.lastTimeProbe > 60000) {
    m_runtime.lastTimeProbe = millis();
    LOG_WARN("TIME", F("NTP stuck. Probing HTTP server for 'Date' header..."));
    (void)probeServerTimeHeader(true);
  }
}

bool ApiClientTransportController::probeServerTimeHeader(bool allowInsecure) {
  if (!acquireTlsResources(allowInsecure)) {
    m_deps.configManager.releaseStrings();
    return false;
  }

  updateCloudTargetCache();
  m_deps.configManager.releaseStrings();

  char fallbackHost[sizeof("example.com")];
  char fallbackPath[sizeof("/api/sensor")];
  copy_trunc_P(fallbackHost, sizeof(fallbackHost), PSTR("example.com"));
  copy_trunc_P(fallbackPath, sizeof(fallbackPath), PSTR("/api/sensor"));
  const char* host = (m_transport.cloudHost[0] != '\0') ? m_transport.cloudHost : fallbackHost;
  const char* path = (m_transport.cloudPath[0] != '\0') ? m_transport.cloudPath : fallbackPath;

  if (!m_deps.secureClient.connect(host, 443)) {
    m_deps.secureClient.stop();
    releaseTlsResources();
    m_deps.configManager.releaseStrings();
    return false;
  }

  char authBuf[kBearerHeaderBufferLen];
  const bool hasAuthHeader = REDACTED
  char userAgent[NodeIdentity::kUserAgentBufferLen];
  NodeIdentity::buildUserAgent(userAgent, sizeof(userAgent));
  char deviceId[NodeIdentity::kDeviceIdBufferLen];
  NodeIdentity::buildDeviceId(deviceId, sizeof(deviceId));

  m_deps.secureClient.print(F("HEAD "));
  m_deps.secureClient.print(path);
  m_deps.secureClient.print(F(" HTTP/1.1\r\nHost: "));
  m_deps.secureClient.print(host);
  m_deps.secureClient.print(F("\r\nConnection: close\r\nAccept: */*\r\nUser-Agent: "));
  m_deps.secureClient.print(userAgent);
  m_deps.secureClient.print(F("\r\nX-Device-ID: "));
  m_deps.secureClient.print(deviceId);
  m_deps.secureClient.print(F("\r\n"));
  if (hasAuthHeader) {
    m_deps.secureClient.print(F("Authorization: REDACTED
    m_deps.secureClient.print(authBuf);
    m_deps.secureClient.print(F("\r\n"));
  }
  m_deps.secureClient.print(F("\r\n"));

  char line[128];
  if (!read_line(m_deps.secureClient, line, sizeof(line), m_policy.secureLineTimeoutMs)) {
    m_deps.secureClient.stop();
    releaseTlsResources();
    m_deps.configManager.releaseStrings();
    return false;
  }

  char dateBuf[64] = {0};
  parse_response_headers(m_deps.secureClient, dateBuf, sizeof(dateBuf), nullptr, 0);

  const bool wasSynced = m_deps.ntpClient.isTimeSynced();
  sync_time_from_http_date(m_deps.ntpClient, dateBuf);
  const bool updated = (!wasSynced && m_deps.ntpClient.isTimeSynced());

  m_deps.secureClient.stop();
  releaseTlsResources();
  m_deps.configManager.releaseStrings();
  return updated;
}

void ApiClientTransportController::updateCloudTargetCache() {
  updateCloudTargetCacheFor(shouldUseRelayForCloudUpload());
}

void ApiClientTransportController::updateCloudTargetCacheFor(bool useRelay) {
  const char* url = useRelay ? DEFAULT_RELAY_DATA_URL : m_deps.configManager.getDataUploadUrl();
  resolveCloudTarget(url, m_transport.cloudHost, sizeof(m_transport.cloudHost), m_transport.cloudPath, sizeof(m_transport.cloudPath));
  m_runtime.route.cloudTargetIsRelay = useRelay;
  m_deps.configManager.releaseStrings();
}

void ApiClientTransportController::activateRelayFallback() {
  if (m_runtime.route.uplinkMode != UplinkMode::AUTO) {
    return;
  }
  m_runtime.route.forceRelayNextCloudAttempt = true;
  m_runtime.route.relayPinnedUntil = millis() + RELAY_FALLBACK_PIN_MS;
  updateCloudTargetCacheFor(true);
}

void ApiClientTransportController::clearRelayFallback() {
  m_runtime.route.forceRelayNextCloudAttempt = false;
  m_runtime.route.relayPinnedUntil = 0;
  if (m_runtime.route.cloudTargetIsRelay && m_runtime.route.uplinkMode != UplinkMode::RELAY) {
    updateCloudTargetCacheFor(false);
  }
}

void ApiClientTransportController::buildErrorMessage(UploadResult& result) {
  if (strncmp_P(result.message, PSTR("Imunify360 blocked"), sizeof(result.message)) == 0) {
    result.success = false;
    return;
  }
  if (result.success) {
    copy_trunc_P(result.message, sizeof(result.message), PSTR("OK"));
    return;
  }

  if (result.httpCode < 0) {
    result.message[0] = '\0';
    size_t pos = 0;
    pos = append_literal_P(result.message, sizeof(result.message), pos, PSTR("HTTP error "));
    pos = append_i32(result.message, sizeof(result.message), pos, result.httpCode);
    return;
  }
  PGM_P reason = lookup_http_reason_P(result.httpCode);

  if (result.httpCode >= 300 && result.httpCode < 400 && m_transport.lastResponseLocation[0] != '\0') {
    char display[24];
    copy_location_display(display, sizeof(display), m_transport.lastResponseLocation);
    result.message[0] = '\0';
    size_t pos = 0;
    pos = append_literal_P(result.message, sizeof(result.message), pos, reason);
    pos = append_literal_P(result.message, sizeof(result.message), pos, PSTR("->"));
    pos = append_literal(result.message, sizeof(result.message), pos, display);
    return;
  }

  result.message[0] = '\0';
  size_t pos = 0;
  pos = append_literal_P(result.message, sizeof(result.message), pos, PSTR("HTTP "));
  pos = append_i32(result.message, sizeof(result.message), pos, result.httpCode);
  pos = append_literal_P(result.message, sizeof(result.message), pos, PSTR(" ("));
  pos = append_literal_P(result.message, sizeof(result.message), pos, reason);
  pos = append_literal_P(result.message, sizeof(result.message), pos, PSTR(")"));
}

void ApiClientTransportController::signPayload(const char* payload, size_t payload_len, char* signatureBuffer) {
  br_hmac_key_context kc;
  br_hmac_context ctx;

  const char* token = REDACTED
  const size_t token_len = REDACTED
  if (token_len =REDACTED
    if (signatureBuffer) {
      signatureBuffer[0] = '\0';
    }
    m_deps.configManager.releaseStrings();
    LOG_ERROR("REDACTED", F("REDACTED"));
    return;
  }
  if (!payload || payload_len == 0) {
    if (signatureBuffer) {
      signatureBuffer[0] = '\0';
    }
    m_deps.configManager.releaseStrings();
    LOG_ERROR("API", F("Payload empty; cannot sign"));
    return;
  }

  br_hmac_key_init(&kc, &br_sha256_vtable, token, token_len);
  br_hmac_init(&ctx, &kc, 0);
  br_hmac_update(&ctx, payload, payload_len);

  uint8_t digest[32];
  br_hmac_out(&ctx, digest);

  static const char hex[] = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    signatureBuffer[i * 2] = hex[(digest[i] >> 4) & 0x0F];
    signatureBuffer[i * 2 + 1] = hex[digest[i] & 0x0F];
  }
  signatureBuffer[64] = '\0';
  m_deps.configManager.releaseStrings();
}

bool ApiClientTransportController::executeQosSample(HTTPClient& http,
                                                    const char* url,
                                                    const char* method,
                                                    const char* payload,
                                                    bool useOtaToken,
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

  if (http.begin(m_deps.secureClient, url)) {
    char path[8] = {0};
    resolveCloudTarget(url, nullptr, 0, path, sizeof(path));

    char authBuf[kBearerHeaderBufferLen];
    const bool hasAuthHeader = REDACTED
                                           : build_auth_header_for_upload(authBuf, sizeof(authBuf), m_deps.configManager);
    if (hasAuthHeader) {
      http.addHeader(F("REDACTED"), authBuf);
    }
    char userAgent[NodeIdentity::kUserAgentBufferLen];
    NodeIdentity::buildUserAgent(userAgent, sizeof(userAgent));
    char deviceId[NodeIdentity::kDeviceIdBufferLen];
    NodeIdentity::buildDeviceId(deviceId, sizeof(deviceId));
    http.addHeader(F("User-Agent"), userAgent);
    http.addHeader(F("X-Device-ID"), deviceId);

    if (strcmp_P(method, PSTR("POST")) == 0) {
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
