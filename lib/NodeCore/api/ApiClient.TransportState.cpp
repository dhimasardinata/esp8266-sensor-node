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

void ApiClientTransportController::startUpload(const char* payload, size_t length, bool isEdgeTarget) {
  if (m_transport.httpState != HttpState::IDLE) {
    LOG_WARN("API", F("Upload request ignored - Busy"));
    return;
  }

  m_transport.payloadLen = length;
  m_runtime.route.targetIsEdge = isEdgeTarget;
  if (!m_runtime.route.targetIsEdge) {
    const bool useRelay = shouldUseRelayForCloudUpload();
    updateCloudTargetCacheFor(useRelay);
    if (useRelay && m_runtime.route.uplinkMode == UplinkMode::AUTO && m_runtime.route.forceRelayNextCloudAttempt) {
      m_runtime.route.forceRelayNextCloudAttempt = false;
    }
  }
  m_transport.lastResponseLocation[0] = '\0';
  (void)payload;

  transitionState(HttpState::CONNECTING);
}

void ApiClientTransportController::handleStateConnecting(const AppConfig& cfg) {
  bool isEdge = m_runtime.route.targetIsEdge;

  const char* host = nullptr;
  EdgeGatewayTargets edgeTargets{};
  uint16_t port = 0;
  m_transport.edgeHost[0] = '\0';

  if (isEdge) {
    edgeTargets = resolveEdgeGatewayTargets(m_deps.configManager);
    host = edgeTargets.primaryMdns;
    port = 80;
    m_transport.activeClient = &m_transport.plainClient;
  } else {
    updateCloudTargetCache();
    host = m_transport.cloudHost;
    port = 443;
    m_transport.activeClient = &m_deps.secureClient;
  }

  broadcastUploadTarget(isEdge);

  if (!isEdge) {
    if (!acquireTlsResources(cfg.ALLOW_INSECURE_HTTPS())) {
      updateResult_P(HTTPC_ERROR_TOO_LESS_RAM, false, PSTR("Low TLS heap"));
      transitionState(HttpState::FAILED);
      return;
    }
    const ApiClientHealth::HeapBudget budget = ApiClientHealth::captureTlsHeapBudget(m_ctx);
    if (!budget.healthy) {
      updateResult_P(HTTPC_ERROR_TOO_LESS_RAM, false, PSTR("Low TLS heap"));
      transitionState(HttpState::FAILED);
      return;
    }
  }
  m_transport.activeClient->setTimeout(m_policy.connectTimeoutMs);
  yield();
  LOG_INFO("API", F("TLS pre-connect heap: %u, blk: %u"), ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
  bool connected = false;
  if (isEdge) {
    const char* candidates[] = {
        edgeTargets.primaryMdns,
        edgeTargets.primaryIp,
        edgeTargets.secondaryMdns,
        edgeTargets.secondaryIp,
    };

    for (size_t i = 0; i < (sizeof(candidates) / sizeof(candidates[0])) && !connected; ++i) {
      const char* candidate = candidates[i];
      if (!candidate || candidate[0] == '\0') {
        continue;
      }

      bool duplicate = false;
      for (size_t j = 0; j < i; ++j) {
        const char* prev = candidates[j];
        if (prev && strcmp(prev, candidate) == 0) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) {
        continue;
      }

      if (i > 0) {
        char label[17];
        copy_trunc_P(label, sizeof(label), edge_target_label_P(i));
        LOG_WARN("API", F("Edge fallback -> %s"), label);
      }
      connected = m_transport.activeClient->connect(candidate, port);
      if (connected) {
        host = candidate;
      }
    }

    if (!connected) {
      LOG_WARN("API", F("Edge gateways unreachable; trying cloud fallback"));
    }
  } else {
    connected = m_transport.activeClient->connect(host, port);
  }

  if (connected) {
    if (isEdge) {
      copy_trunc(m_transport.edgeHost, sizeof(m_transport.edgeHost), host);
    }
    transitionState(HttpState::SENDING_REQUEST);
  } else {
    updateResult_P(HTTPC_ERROR_CONNECTION_FAILED, false, PSTR("Connect Failed"));
    transitionState(HttpState::FAILED);
  }
}

void ApiClientTransportController::handleStateSending(const AppConfig& cfg) {
  if (!m_transport.activeClient || !m_transport.activeClient->connected()) {
    updateResult_P(HTTPC_ERROR_CONNECTION_LOST, false, PSTR("Disconnected"));
    transitionState(HttpState::FAILED);
    return;
  }
  char* buf = sharedBuffer();
  if (!buf) {
    updateResult_P(HTTPC_ERROR_CONNECTION_LOST, false, PSTR("No payload buffer"));
    transitionState(HttpState::FAILED);
    return;
  }

  if (!m_runtime.route.targetIsEdge) {
    updateCloudTargetCache();
  }

  char edgePath[sizeof("/api/data")];
  copy_trunc_P(edgePath, sizeof(edgePath), PSTR("/api/data"));
  const char* path = m_runtime.route.targetIsEdge ? edgePath : m_transport.cloudPath;
  EdgeGatewayTargets edgeTargets = resolveEdgeGatewayTargets(m_deps.configManager);
  const char* host = m_runtime.route.targetIsEdge
                         ? ((m_transport.edgeHost[0] != '\0') ? m_transport.edgeHost : edgeTargets.primaryMdns)
                         : m_transport.cloudHost;
  char userAgent[NodeIdentity::kUserAgentBufferLen];
  NodeIdentity::buildUserAgent(userAgent, sizeof(userAgent));
  char deviceId[NodeIdentity::kDeviceIdBufferLen];
  NodeIdentity::buildDeviceId(deviceId, sizeof(deviceId));

  m_transport.activeClient->print(F("POST "));
  m_transport.activeClient->print(path);
  m_transport.activeClient->print(F(" HTTP/1.1\r\nHost: "));
  m_transport.activeClient->print(host);
  m_transport.activeClient->print(
      F("\r\nConnection: close\r\n"
        "Content-Type: application/json\r\n"
        "Accept: application/json\r\n"
        "User-Agent: "));
  m_transport.activeClient->print(userAgent);
  m_transport.activeClient->print(F("\r\nX-Device-ID: "));
  m_transport.activeClient->print(deviceId);
  m_transport.activeClient->print(F("\r\nContent-Length: "));
  m_transport.activeClient->print(m_transport.payloadLen);
  m_transport.activeClient->print(F("\r\n"));

  if (m_runtime.route.targetIsEdge) {
    m_transport.activeClient->print(F("X-Node-ID: "));
    m_transport.activeClient->print(NODE_ID);
    m_transport.activeClient->print(F("\r\n"));

    m_transport.activeClient->print(F("X-GH-ID: "));
    m_transport.activeClient->print(GH_ID);
    m_transport.activeClient->print(F("\r\n"));

    char signature[65];
    signPayload(buf, m_transport.payloadLen, signature);
    m_transport.activeClient->print(F("X-Signature: "));
    m_transport.activeClient->print(signature);
    m_transport.activeClient->print(F("\r\n"));

    m_transport.activeClient->print(F("X-Timestamp: "));
    m_transport.activeClient->print((unsigned long)time(nullptr));
    m_transport.activeClient->print(F("\r\n"));
  } else {
    char authBuf[kBearerHeaderBufferLen];
    if (build_auth_header_for_upload(authBuf, sizeof(authBuf), m_deps.configManager)) {
      m_transport.activeClient->print(F("Authorization: REDACTED
      m_transport.activeClient->print(authBuf);
      m_transport.activeClient->print(F("\r\n"));
    }
  }

  m_transport.activeClient->print(F("\r\n"));

  const StreamWriteResult writeResult =
      write_all(*m_transport.activeClient, reinterpret_cast<const uint8_t*>(buf), m_transport.payloadLen, m_policy.writeTimeoutMs);
  releaseSharedBuffer();

  if (writeResult.written < m_transport.payloadLen) {
    updateResult_P(writeResult.disconnected ? HTTPC_ERROR_CONNECTION_LOST : HTTPC_ERROR_SEND_PAYLOAD_FAILED,
                   false,
                   writeResult.timedOut ? PSTR("Write timeout")
                                        : writeResult.disconnected ? PSTR("Connection Lost")
                                                                   : PSTR("Short write"));
    transitionState(HttpState::FAILED);
    return;
  }

  transitionState(HttpState::WAITING_RESPONSE);
}

void ApiClientTransportController::handleStateWaiting(unsigned long stateDuration) {
  if (!m_transport.activeClient) {
    updateResult_P(HTTPC_ERROR_CONNECTION_LOST, false, PSTR("Connection Lost"));
    transitionState(HttpState::FAILED);
    return;
  }

  if (m_transport.activeClient->available()) {
    transitionState(HttpState::READING_RESPONSE);
  } else {
    if (!m_transport.activeClient->connected()) {
      updateResult_P(HTTPC_ERROR_CONNECTION_LOST, false, PSTR("Connection Lost"));
      transitionState(HttpState::FAILED);
      return;
    }
    if (stateDuration > m_policy.waitResponseTimeoutMs) {
      updateResult_P(HTTPC_ERROR_READ_TIMEOUT, false, PSTR("Timeout"));
      transitionState(HttpState::FAILED);
    }
  }
}

void ApiClientTransportController::handleStateReading() {
  char line[128];
  size_t n = m_transport.activeClient->readBytesUntil('\n', line, sizeof(line) - 1);
  line[n] = '\0';
  Utils::trim_inplace(std::span<char>(line));

  const char* p = strchr(line, ' ');
  if (!p) {
    updateResult_P(-1, false, PSTR("Bad Response"));
  } else {
    while (*p == ' ') {
      ++p;
    }
    if (*p < '0' || *p > '9') {
      updateResult_P(-1, false, PSTR("Bad Response"));
    } else {
      int code = 0;
      while (*p >= '0' && *p <= '9') {
        code = (code * 10) + (*p - '0');
        ++p;
      }
      m_transport.lastResult.httpCode = code;
      m_transport.lastResult.success = (code >= 200 && code < 300);
      char dateBuf[64] = {0};
      m_transport.lastResponseLocation[0] = '\0';
      parse_response_headers(*m_transport.activeClient,
                             dateBuf,
                             sizeof(dateBuf),
                             m_transport.lastResponseLocation,
                             sizeof(m_transport.lastResponseLocation));
      sync_time_from_http_date(m_deps.ntpClient, dateBuf);

      char bodyPreview[192] = {0};
      const size_t bodyLen =
          read_body_preview(*m_transport.activeClient, bodyPreview, sizeof(bodyPreview), m_policy.previewTimeoutMs);
      const bool wafBlocked = (bodyLen > 0 && response_body_indicates_waf_block(bodyPreview));
      if (wafBlocked) {
        m_transport.lastResult.success = false;
        if (m_transport.lastResult.httpCode >= 200 && m_transport.lastResult.httpCode < 300) {
          m_transport.lastResult.httpCode = 403;
        }
        copy_trunc_P(m_transport.lastResult.message, sizeof(m_transport.lastResult.message), PSTR("Imunify360 blocked"));
      }

      if (m_transport.lastResult.success) {
        copy_trunc_P(m_transport.lastResult.message, sizeof(m_transport.lastResult.message), PSTR("OK"));
      } else if (!wafBlocked) {
        if (m_transport.lastResponseLocation[0] != '\0') {
          char display[24];
          copy_location_display(display, sizeof(display), m_transport.lastResponseLocation);
          LOG_WARN("API", F("Redirect to: %s"), m_transport.lastResponseLocation);
          m_transport.lastResult.message[0] = '\0';
          size_t pos = 0;
          pos = append_literal_P(m_transport.lastResult.message, sizeof(m_transport.lastResult.message), pos, PSTR("HTTP "));
          pos = append_i32(m_transport.lastResult.message, sizeof(m_transport.lastResult.message), pos, code);
          pos = append_literal_P(m_transport.lastResult.message, sizeof(m_transport.lastResult.message), pos, PSTR("->"));
          pos = append_literal(m_transport.lastResult.message, sizeof(m_transport.lastResult.message), pos, display);
        } else {
          m_transport.lastResult.message[0] = '\0';
          size_t pos = 0;
          pos = append_literal_P(m_transport.lastResult.message, sizeof(m_transport.lastResult.message), pos, PSTR("HTTP "));
          pos = append_i32(m_transport.lastResult.message, sizeof(m_transport.lastResult.message), pos, code);
        }
      }
    }
  }

  m_transport.activeClient->stop();
  transitionState(HttpState::COMPLETE);
}

void ApiClientTransportController::handleUploadStateMachine() {
  const auto& cfg = m_deps.configManager.getConfig();
  unsigned long stateDuration = millis() - m_transport.stateEntryTime;

  switch (m_transport.httpState) {
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
