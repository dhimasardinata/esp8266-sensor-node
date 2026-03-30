#include "api/ApiClient.UploadController.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <bearssl/bearssl_hmac.h>
#include <user_interface.h>

#include <algorithm>
#include <array>

#include "storage/CacheManager.h"
#include "support/CompileTimeJSON.h"
#include "system/ConfigManager.h"
#include "support/CryptoUtils.h"
#include "system/Logger.h"
#include "net/NtpClient.h"
#include "system/NodeIdentity.h"
#include "sensor/SensorManager.h"
#include "REDACTED"
#include "config/constants.h"
#include "generated/node_config.h"
#include "sensor/SensorData.h"
#include "support/Utils.h"
#include "storage/RtcManager.h"

#include "api/ApiClient.Health.h"
#include "api/ApiClient.UploadShared.h"

using namespace ApiClientUploadShared;

// ApiClient.UploadFlow.cpp - upload target dispatch, recovery, and queue completion helpers

void ApiClientUploadController::buildLocalGatewayUrl(char* buffer, size_t bufferSize) {
  if (!buffer || bufferSize == 0) {
    return;
  }
  char dataPath[sizeof("/api/data")];
  copy_trunc_P(dataPath, sizeof(dataPath), PSTR("/api/data"));
  if (!build_gateway_url(buffer, bufferSize, m_api.m_deps.configManager, dataPath)) {
    buffer[0] = '\0';
  }
}

UploadResult ApiClientUploadController::performLocalGatewayUpload(const char* payload, size_t length) {
  UploadResult result{};
  result.httpCode = HTTPC_ERROR_CONNECTION_FAILED;
  copy_trunc_P(result.message, sizeof(result.message), PSTR("Gateway Fail"));
  if (!payload || length == 0) {
    result.httpCode = HTTPC_ERROR_CONNECTION_FAILED;
    copy_trunc_P(result.message, sizeof(result.message), PSTR("No payload"));
    return result;
  }

  ApiClientHealth::refreshRuntimeHealth(m_ctx);
  if (m_health.wifiScanBusy) {
    result.httpCode = HTTPC_ERROR_CONNECTION_LOST;
    copy_trunc_P(result.message, sizeof(result.message), PSTR("REDACTED"));
    return result;
  }

  if (!m_api.m_transport.httpClient) {
    m_api.m_transport.httpClient.reset(new (std::nothrow) HTTPClient());
  }
  if (!m_api.m_transport.httpClient) {
    result.httpCode = HTTPC_ERROR_TOO_LESS_RAM;
    copy_trunc_P(result.message, sizeof(result.message), PSTR("HTTP alloc fail"));
    return result;
  }

  char gatewayUrls[4][MAX_URL_LEN] = {{0}};
  char dataPath[sizeof("/api/data")];
  copy_trunc_P(dataPath, sizeof(dataPath), PSTR("/api/data"));
  const size_t gatewayUrlCount = build_gateway_url_candidates(gatewayUrls, 4, m_api.m_deps.configManager, dataPath);
  if (gatewayUrlCount == 0) {
    result.httpCode = HTTPC_ERROR_CONNECTION_FAILED;
    copy_trunc_P(result.message, sizeof(result.message), PSTR("Gateway URL fail"));
    return result;
  }

  m_api.m_transport.httpClient->setReuse(false);
  m_transport.httpClient->setTimeout(m_policy.edgeHttpTimeoutMs);
  for (size_t i = 0; i < gatewayUrlCount; ++i) {
    const char* gatewayUrl = gatewayUrls[i];
    if (!gatewayUrl || gatewayUrl[0] == '\0') {
      continue;
    }

    if (!m_api.m_transport.httpClient->begin(m_api.m_transport.plainClient, gatewayUrl)) {
      result.httpCode = HTTPC_ERROR_CONNECTION_FAILED;
      copy_trunc_P(result.message, sizeof(result.message), PSTR("Gateway begin fail"));
      continue;
    }

    m_api.m_transport.httpClient->addHeader(F("Content-Type"), F("application/json"));
    char userAgent[NodeIdentity::kUserAgentBufferLen];
    NodeIdentity::buildUserAgent(userAgent, sizeof(userAgent));
    char deviceId[NodeIdentity::kDeviceIdBufferLen];
    NodeIdentity::buildDeviceId(deviceId, sizeof(deviceId));
    m_api.m_transport.httpClient->addHeader(F("User-Agent"), userAgent);
    m_api.m_transport.httpClient->addHeader(F("X-Device-ID"), deviceId);
    const int httpCode = m_api.m_transport.httpClient->POST(reinterpret_cast<const uint8_t*>(payload), length);
    result.httpCode = static_cast<int16_t>(httpCode);
    result.success = (httpCode >= 200 && httpCode < 300);
    if (result.success) {
      copy_trunc_P(result.message, sizeof(result.message), PSTR("OK (Edge)"));
      m_api.m_transport.httpClient->end();
      return result;
    }
    snprintf_P(result.message, sizeof(result.message), PSTR("HTTP %d"), httpCode);
    m_api.m_transport.httpClient->end();
  }

  return result;
}

void ApiClientUploadController::resetSampleAccumulator() {
  m_api.m_runtime.rssiSum = 0;
  m_api.m_runtime.sampleCount = 0;
}

void ApiClientUploadController::clearCurrentRecordFlags() {
  m_api.m_runtime.route.currentRecordSentToGateway = false;
  m_api.m_runtime.route.forceCloudAfterEdgeFailure = false;
}

void ApiClientUploadController::clearLoadedRecordContext() {
  clearCurrentRecordFlags();
  m_api.m_runtime.route.loadedRecordSource = ApiClient::UploadRecordSource::NONE;
}

void ApiClientUploadController::resetQueuePopRecovery() {
  m_api.m_runtime.queue.popFailStreak = 0;
  m_api.m_runtime.queue.popRetryAfter = 0;
}

void ApiClientUploadController::resetRtcFallbackRecovery() {
  m_api.m_runtime.queue.rtcFallbackFsFailStreak = 0;
  m_api.m_runtime.queue.rtcFallbackFsRetryAfter = 0;
}

void ApiClientUploadController::copyCurrentUploadSourceLabel(char* out, size_t out_len) const {
  m_api.copyUploadSourceLabel(out, out_len, m_api.m_runtime.route.loadedRecordSource);
}

void ApiClientUploadController::broadcastUploadDispatch(bool immediate, bool isTargetEdge, size_t payload_len) {
  char sourceText[12];
  copyCurrentUploadSourceLabel(sourceText, sizeof(sourceText));
  char msg[112];
  int n = snprintf_P(msg,
                     sizeof(msg),
                     immediate
                         ? (isTargetEdge ? PSTR("[UPLOAD] Immediate EDGE send from %s (%u B)")
                                         : PSTR("[UPLOAD] Immediate HTTPS send from %s (%u B)"))
                         : (isTargetEdge ? PSTR("[UPLOAD] EDGE send from %s (%u B)")
                                         : PSTR("[UPLOAD] HTTPS send from %s (%u B)")),
                     sourceText,
                     static_cast<unsigned>(payload_len));
  if (n > 0) {
    const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
    m_api.broadcastEncrypted(std::string_view(msg, len));
  }
}

bool ApiClientUploadController::finishLoadedRecordSuccess(const AppConfig& cfg,
                                                          int httpCode,
                                                          bool setIdleOnPopFailure) {
  const ApiClient::UploadRecordSource uploadedFrom = m_api.m_runtime.route.loadedRecordSource;
  if (!m_api.popLoadedRecord()) {
    if (setIdleOnPopFailure) {
      m_api.m_runtime.uploadState = ApiClient::UploadState::IDLE;
    }
    char sourceText[12];
    m_api.copyUploadSourceLabel(sourceText, sizeof(sourceText), uploadedFrom);
    m_api.applyQueuePopFailureCooldown(cfg, sourceText);
    return false;
  }

  if (m_api.m_runtime.queue.popFailStreak > 0 || m_api.m_runtime.queue.popRetryAfter != 0) {
    m_api.m_runtime.cacheSendTimer.setInterval(cfg.CACHE_SEND_INTERVAL_MS);
  }
  resetQueuePopRecovery();

  char msg[80];
  msg[0] = '\0';
  size_t pos = 0;
  pos = append_literal_P(msg, sizeof(msg), pos, PSTR("[SYSTEM] Upload OK (HTTP "));
  pos = append_i32(msg, sizeof(msg), pos, httpCode);
  pos = append_literal_P(msg, sizeof(msg), pos, PSTR(") via "));
  pos = append_literal_P(msg,
                         sizeof(msg),
                         pos,
                         uploadedFrom == ApiClient::UploadRecordSource::RTC ? PSTR("RTC")
                         : uploadedFrom == ApiClient::UploadRecordSource::LITTLEFS ? PSTR("LittleFS")
                                                                                   : PSTR("Unknown"));
  if (pos > 0) {
    m_api.broadcastEncrypted(std::string_view(msg, pos));
  }
  m_api.m_runtime.route.loadedRecordSource = ApiClient::UploadRecordSource::NONE;
  return true;
}

void ApiClientUploadController::resetQueuedUploadCycle(bool resetTimer) {
  m_api.m_runtime.uploadState = ApiClient::UploadState::IDLE;
  if (resetTimer) {
    m_api.m_runtime.cacheSendTimer.reset();
  }
  m_api.releaseSharedBuffer();
}

bool ApiClientUploadController::recoverPendingQueuePop(const AppConfig& cfg, bool immediate, UploadResult* result) {
  if (m_api.m_runtime.queue.popFailStreak == 0 ||
      m_api.m_runtime.route.loadedRecordSource == ApiClient::UploadRecordSource::NONE) {
    return false;
  }

  char sourceText[12];
  m_api.copyUploadSourceLabel(sourceText, sizeof(sourceText), m_api.m_runtime.route.loadedRecordSource);
  if (m_api.popLoadedRecord()) {
    resetQueuePopRecovery();
    m_api.m_runtime.route.loadedRecordSource = ApiClient::UploadRecordSource::NONE;
    m_api.m_runtime.cacheSendTimer.setInterval(cfg.CACHE_SEND_INTERVAL_MS);
    m_api.m_runtime.cacheSendTimer.reset();
    if (immediate) {
      m_api.broadcastEncrypted(F("[QUEUE] Recovery OK (immediate): pop succeeded."));
      if (result) {
        copy_trunc_P(result->message, sizeof(result->message), PSTR("Queue pop recovered"));
      }
    } else {
      char msg[96];
      int n = snprintf_P(msg, sizeof(msg), PSTR("[QUEUE] Recovery OK: pop %s succeeded."), sourceText);
      if (n > 0) {
        const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
        m_api.broadcastEncrypted(std::string_view(msg, len));
      }
    }
  } else {
    m_api.applyQueuePopFailureCooldown(cfg, sourceText);
    if (immediate && result) {
      copy_trunc_P(result->message, sizeof(result->message), PSTR("Queue pop retry"));
    }
  }
  return true;
}

ApiClient::QueuedUploadTargetDecision ApiClientUploadController::resolveQueuedUploadTarget(bool& isTargetEdge) {
  isTargetEdge = false;
  int gwMode = -1;

  if (m_api.m_runtime.route.uploadMode == UploadMode::AUTO) {
    gwMode = m_api.m_runtime.route.cachedGatewayMode;
    const unsigned long nowMs = millis();
    if (gwMode < 0 || (nowMs - m_api.m_runtime.route.lastGatewayModeCheck) >= ApiClient::GATEWAY_MODE_TTL_MS) {
      gwMode = m_api.checkGatewayMode();
      m_api.m_runtime.route.cachedGatewayMode = static_cast<int8_t>(gwMode);
      m_api.m_runtime.route.lastGatewayModeCheck = nowMs;
    }

    if (gwMode == 0) {
      if (m_api.m_runtime.route.localGatewayMode) {
        m_api.m_runtime.route.localGatewayMode = false;
        LOG_INFO("MODE", F("Gateway enforced CLOUD mode"));
      }
    } else if (gwMode == 1) {
      if (!m_api.m_runtime.route.localGatewayMode) {
        m_api.m_runtime.route.localGatewayMode = true;
        LOG_INFO("MODE", F("Gateway enforced LOCAL mode"));
      }
    }
  }

  if (m_api.m_runtime.route.uploadMode == UploadMode::EDGE) {
    return ApiClient::QueuedUploadTargetDecision::HOLD;
  }

  if (m_api.m_runtime.route.uploadMode == UploadMode::AUTO && m_api.m_runtime.route.localGatewayMode) {
    if (gwMode == 1) {
      return ApiClient::QueuedUploadTargetDecision::HOLD;
    }
    if (millis() - m_api.m_runtime.route.lastCloudRetryAttempt >= AppConstants::CLOUD_RETRY_INTERVAL_MS) {
      LOG_INFO("UPLOAD", F("Auto Mode: Retrying cloud..."));
      m_api.m_runtime.route.lastCloudRetryAttempt = millis();
      return ApiClient::QueuedUploadTargetDecision::PROCEED;
    }
    return ApiClient::QueuedUploadTargetDecision::WAIT;
  }

  return ApiClient::QueuedUploadTargetDecision::PROCEED;
}
