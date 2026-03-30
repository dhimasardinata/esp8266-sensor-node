#include "api/ApiClient.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecureBearSSL.h>
#include <bearssl/bearssl_hmac.h>
#include <user_interface.h>

#include <algorithm>
#include <cstring>
#include <strings.h>

#include "storage/CacheManager.h"  // Concrete type for CRTP
#include "support/CompileTimeJSON.h"
#include "system/ConfigManager.h"
#include "support/CryptoUtils.h"
#include "system/Logger.h"
#include "net/NtpClient.h"
#include "storage/RtcManager.h"
#include "sensor/SensorManager.h"  // Concrete type for CRTP
#include "REDACTED"
#include "config/constants.h"
#include "generated/node_config.h"

#include "api/ApiClient.Health.h"
// ApiClient.Immediate.cpp - immediate upload flow extracted from ApiClient.Transport.cpp

#include "api/ApiClient.TransportShared.h"

using namespace ApiClientTransportShared;

UploadResult ApiClient::performImmediateUpload() {
  UploadResult result = {-1, false, {0}};
  copy_trunc_P(result.message, sizeof(result.message), PSTR("No data"));
  const AppConfig& cfg = m_deps.configManager.getConfig();
  const uint32_t heapBefore = ESP.getFreeHeap();
  const uint32_t blockBefore = ESP.getMaxFreeBlockSize();
  struct ImmediateHeapTrace {
    uint32_t freeBefore;
    uint32_t blockBefore;
    const UploadResult* resultRef;
    ~ImmediateHeapTrace() {
      LOG_INFO("MEM",
               F("Immediate heap trend: before %u/%u -> after %u/%u (code=%d)"),
               freeBefore,
               blockBefore,
               ESP.getFreeHeap(),
               ESP.getMaxFreeBlockSize(),
               resultRef ? resultRef->httpCode : 0);
    }
  } heapTrace{heapBefore, blockBefore, &result};
  (void)heapTrace;

  LOG_INFO("MEM", F("Immediate upload start heap=%u block=%u"), heapBefore, blockBefore);

  if (m_runtime.queue.popRetryAfter != 0 &&
      static_cast<int32_t>(millis() - m_runtime.queue.popRetryAfter) < 0) {
    copy_trunc_P(result.message, sizeof(result.message), PSTR("Queue pop cooldown"));
    return result;
  }

  if (recoverPendingQueuePop(cfg, true, &result)) {
    return result;
  }

  ApiClientHealth::refreshRuntimeHealth(m_context);
  if (m_health.wifiScanBusy) {
    copy_trunc_P(result.message, sizeof(result.message), PSTR("REDACTED"));
    return result;
  }

  size_t record_len = 0;
  if (!prepareImmediateUploadRecord(result, record_len)) {
    return result;
  }
  char* buf = sharedBuffer();

  LOG_INFO("API", F("Immediate upload: %u bytes"), record_len);

  bool isTargetEdge = false;
  if (!resolveImmediateUploadTarget(result, isTargetEdge)) {
    return result;
  }

  broadcastUploadTarget(isTargetEdge);

  if (isTargetEdge) {
    broadcastUploadDispatch(true, true, record_len);
    size_t encLen = prepareEdgePayload(record_len);
    if (encLen > 0) {
      result = performLocalGatewayUpload(buf, encLen);
      if (!result.success) {
        LOG_WARN("API", F("Immediate EDGE gateways failed; trying cloud"));

        size_t cloud_len = 0;
        UploadRecordLoad cloudLoad = loadRecordForUpload(cloud_len);
        if (cloudLoad == UploadRecordLoad::READY && cloud_len > 0) {
          char fallbackSourceText[12];
          copyCurrentUploadSourceLabel(fallbackSourceText, sizeof(fallbackSourceText));
          char cloudMsg[104];
          int cn = snprintf_P(cloudMsg,
                              sizeof(cloudMsg),
                              PSTR("[UPLOAD] Immediate HTTPS fallback from %s (%u B)"),
                              fallbackSourceText,
                              static_cast<unsigned>(cloud_len));
          if (cn > 0) {
            const size_t len = static_cast<size_t>(std::min<int>(cn, static_cast<int>(sizeof(cloudMsg) - 1)));
            broadcastEncrypted(std::string_view(cloudMsg, len));
          }
          result = performSingleUpload(buf, cloud_len, false);
          if (result.httpCode == HTTPC_ERROR_TOO_LESS_RAM) {
            LOG_WARN("MEM", F("Immediate deferred: low RAM after EDGE->CLOUD fallback"));
            markImmediateUploadDeferred(result);
            return result;
          }
        }
      }
      m_transport.httpClient.reset();
    } else {
      result.success = false;
      copy_trunc_P(result.message, sizeof(result.message), PSTR("Encryption failed"));
    }
  } else {
    broadcastUploadDispatch(true, false, record_len);
    result = performSingleUpload(buf, record_len, false);
    if (result.httpCode == HTTPC_ERROR_TOO_LESS_RAM) {
      LOG_WARN("MEM", F("Immediate deferred: low RAM on HTTPS path"));
      markImmediateUploadDeferred(result);
      return result;
    }
  }

  finalizeImmediateUploadResult(result, cfg);
  return result;
}

bool ApiClient::prepareImmediateUploadRecord(UploadResult& result, size_t& record_len) {
  record_len = 0;

  if (!createAndCachePayload()) {
    copy_trunc_P(result.message, sizeof(result.message), PSTR("Payload creation failed"));
    return false;
  }

  char* buf = sharedBuffer();
  const size_t buf_len = sharedBufferSize();
  if (!buf || buf_len == 0) {
    copy_trunc_P(result.message, sizeof(result.message), PSTR("No payload buffer"));
    return false;
  }

  UploadRecordLoad loadStatus = loadRecordForUpload(record_len);
  if (loadStatus == UploadRecordLoad::READY && record_len > 0) {
    return true;
  }
  if (loadStatus == UploadRecordLoad::RETRY) {
    copy_trunc_P(result.message, sizeof(result.message), PSTR("Queue recovery"));
  } else if (loadStatus == UploadRecordLoad::EMPTY) {
    copy_trunc_P(result.message, sizeof(result.message), PSTR("Queue empty"));
  } else {
    copy_trunc_P(result.message, sizeof(result.message), PSTR("Queue read failed"));
  }
  return false;
}

void ApiClient::markImmediateUploadDeferred(UploadResult& result) {
  m_runtime.immediate.warmup = 1;
  m_runtime.immediate.requested = true;
  m_transport.httpClient.reset();
  result.httpCode = kImmediateDeferred;
  result.success = false;
  copy_trunc_P(result.message, sizeof(result.message), PSTR("Deferred"));
}

void ApiClient::resetImmediateUploadPollState() {
  m_runtime.immediate.pollReady = false;
  m_runtime.immediate.gatewayMode = -2;
}

bool ApiClient::resolveImmediateUploadTarget(UploadResult& result, bool& isTargetEdge) {
  isTargetEdge = false;
  int gwMode = -1;
  if (m_runtime.route.uploadMode == UploadMode::AUTO) {
    if (m_runtime.immediate.pollReady) {
      gwMode = m_runtime.immediate.gatewayMode;
    } else {
      const ApiClientHealth::HeapBudget apiBudget = ApiClientHealth::captureApiHeapBudget(m_context);
      if (apiBudget.healthy) {
        gwMode = checkGatewayMode();
        LOG_INFO("MODE", F("Immediate gateway poll: %d (%s)"), gwMode, gatewayModeLabel(gwMode));
      } else {
        LOG_WARN("MODE",
                 F("Immediate gateway poll skipped (low heap: %u, block %u)"),
                 apiBudget.freeHeap,
                 apiBudget.maxBlock);
      }
      m_runtime.immediate.gatewayMode = static_cast<int8_t>(gwMode);
      m_runtime.immediate.pollReady = true;
      if (gwMode != 1) {
        LOG_WARN("MEM",
                 F("Immediate deferred: warmup before HTTPS path (gwMode=%d (%s))"),
                 gwMode,
                 gatewayModeLabel(gwMode));
        markImmediateUploadDeferred(result);
        return false;
      }
    }
    isTargetEdge = (gwMode == 1);
  } else if (m_runtime.route.uploadMode == UploadMode::EDGE) {
    isTargetEdge = true;
  }
  return true;
}

void ApiClient::finalizeImmediateUploadResult(UploadResult& result, const AppConfig& cfg) {
  if (result.success) {
    if (!finishLoadedRecordSuccess(cfg, result.httpCode, false)) {
      LOG_WARN("UPLOAD", F("Immediate upload success but failed to clear source queue"));
    }
    m_runtime.lastApiSuccessMillis = millis();
    m_runtime.consecutiveUploadFailures = 0;
  } else {
    m_runtime.consecutiveUploadFailures++;
  }
  resetImmediateUploadPollState();
}
