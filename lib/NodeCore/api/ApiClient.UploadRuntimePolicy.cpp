#include "api/ApiClient.UploadRuntimeController.h"

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
#include "storage/RtcManager.h"
#include "sensor/SensorManager.h"
#include "REDACTED"
#include "config/constants.h"
#include "generated/node_config.h"
#include "sensor/SensorData.h"
#include "support/Utils.h"

#include "api/ApiClient.Health.h"
#include "api/ApiClient.UploadShared.h"

using namespace ApiClientUploadShared;

void ApiClientUploadRuntimeController::notifyLowMemory(uint32_t maxBlock, uint32_t totalFree) {
  LOG_WARN("MEM", F("Low Mem - Skip. Block: %u, Total: %u"), maxBlock, totalFree);
  char msg[80];
  msg[0] = '\0';
  size_t pos = 0;
  pos = append_literal_P(msg, sizeof(msg), pos, PSTR("[SYSTEM] Upload Skipped: Low RAM (Free: "));
  pos = append_u32(msg, sizeof(msg), pos, totalFree);
  pos = append_literal_P(msg, sizeof(msg), pos, PSTR(", Blk: "));
  pos = append_u32(msg, sizeof(msg), pos, maxBlock);
  pos = append_literal_P(msg, sizeof(msg), pos, PSTR(")"));
  if (pos > 0) {
    m_api.broadcastEncrypted(std::string_view(msg, pos));
  }
}

unsigned long ApiClientUploadRuntimeController::calculateBackoffInterval(const AppConfig& cfg) {
  unsigned long multiplier = 1;
  if (m_api.m_runtime.consecutiveUploadFailures < 16) {
    multiplier = (1UL << m_api.m_runtime.consecutiveUploadFailures);
  } else {
    multiplier = (1UL << 15);
  }

  unsigned long nextInterval = cfg.CACHE_SEND_INTERVAL_MS * multiplier;
  return (nextInterval > ApiClient::MAX_BACKOFF_MS) ? ApiClient::MAX_BACKOFF_MS : nextInterval;
}

void ApiClientUploadRuntimeController::trackUploadFailure() {
  m_api.m_runtime.consecutiveUploadFailures++;

  if (m_api.m_runtime.consecutiveUploadFailures == 5) {
    LOG_WARN("REDACTED", F("REDACTED"));
    WiFi.disconnect(false);
  }
}

void ApiClientUploadRuntimeController::handleSuccessfulUpload(UploadResult& res, const AppConfig& cfg) {
  LOG_INFO("UPLOAD", F("Success: HTTP %d (%s)"), res.httpCode, res.message);
  m_api.m_runtime.lastApiSuccessMillis = millis();
  m_api.m_runtime.swWdtTimer.reset();
  m_api.clearCurrentRecordFlags();

  if (m_api.m_runtime.consecutiveUploadFailures > 0) {
    m_api.m_runtime.consecutiveUploadFailures = 0;
    m_api.m_runtime.cacheSendTimer.setInterval(cfg.CACHE_SEND_INTERVAL_MS);
    LOG_INFO("UPLOAD", F("Backoff reset to normal interval."));
  }

  if (!m_api.m_runtime.route.targetIsEdge &&
      m_api.m_runtime.route.uplinkMode == UplinkMode::AUTO &&
      !m_api.m_runtime.route.cloudTargetIsRelay &&
      (m_api.m_runtime.route.relayPinnedUntil != 0 || m_api.m_runtime.route.forceRelayNextCloudAttempt)) {
    m_api.clearRelayFallback();
    LOG_INFO("UPLOAD", F("Direct uplink recovered. Relay pin cleared."));
    m_api.broadcastEncrypted(F("[SYSTEM] Direct cloud uplink recovered."));
  }

  if (m_api.m_runtime.route.localGatewayMode && !m_api.m_runtime.route.targetIsEdge) {
    m_api.m_runtime.route.localGatewayMode = false;
    LOG_INFO("UPLOAD", F("Cloud recovered! Exiting gateway mode."));
    m_api.broadcastEncrypted(F("[SYSTEM] Cloud API recovered. Normal mode restored."));
  }

  (void)m_api.finishLoadedRecordSuccess(cfg, res.httpCode, true);
}

void ApiClientUploadRuntimeController::handleFailedUpload(UploadResult& res, const AppConfig& cfg) {
  LOG_WARN("UPLOAD", F("Failed: %d (%s)"), res.httpCode, res.message);
  char msg[80];
  msg[0] = '\0';
  size_t pos = 0;
  pos = append_literal_P(msg, sizeof(msg), pos, PSTR("[SYSTEM] Fail: "));
  pos = append_cstr(msg, sizeof(msg), pos, res.message);
  pos = append_literal_P(msg, sizeof(msg), pos, PSTR(" ("));
  pos = append_i32(msg, sizeof(msg), pos, res.httpCode);
  pos = append_literal_P(msg, sizeof(msg), pos, PSTR(")"));
  if (pos > 0) {
    m_api.broadcastEncrypted(std::string_view(msg, pos));
  }

  if (m_api.shouldFallbackToRelay(res)) {
    m_api.activateRelayFallback();
    LOG_WARN("UPLOAD",
             F("Direct cloud failed: %d (%s). Relay retry armed."),
             res.httpCode,
             res.message);
    char relayMsg[112];
    relayMsg[0] = '\0';
    size_t relayPos = 0;
    relayPos = append_literal_P(relayMsg, sizeof(relayMsg), relayPos, PSTR("[SYSTEM] DIRECT FAIL: "));
    relayPos = append_cstr(relayMsg, sizeof(relayMsg), relayPos, res.message);
    relayPos = append_literal_P(relayMsg, sizeof(relayMsg), relayPos, PSTR(" ("));
    relayPos = append_i32(relayMsg, sizeof(relayMsg), relayPos, res.httpCode);
    relayPos = append_literal_P(relayMsg, sizeof(relayMsg), relayPos, PSTR(") -> RELAY"));
    if (relayPos > 0) {
      m_api.broadcastEncrypted(std::string_view(relayMsg, relayPos));
    }
    m_api.m_runtime.uploadState = ApiClient::UploadState::IDLE;
    m_api.m_runtime.cacheSendTimer.setInterval(
        std::min<unsigned long>(cfg.CACHE_SEND_INTERVAL_MS, ApiClient::RELAY_RETRY_DELAY_MS));
    m_api.m_runtime.cacheSendTimer.reset();
    return;
  }

  m_api.m_runtime.uploadState = ApiClient::UploadState::IDLE;
  m_api.m_runtime.cacheSendTimer.reset();

  trackUploadFailure();

  unsigned long nextInterval = calculateBackoffInterval(cfg);
  m_api.m_runtime.cacheSendTimer.setInterval(nextInterval);

  LOG_WARN("UPLOAD",
           F("Backoff active. Failures: %u. Next retry in: %lu s"),
           m_api.m_runtime.consecutiveUploadFailures,
           nextInterval / 1000);

  if (m_api.m_runtime.route.uploadMode == UploadMode::AUTO && !m_api.m_runtime.route.localGatewayMode) {
    if (m_api.m_runtime.consecutiveUploadFailures >= AppConstants::LOCAL_GATEWAY_FALLBACK_THRESHOLD) {
      m_api.m_runtime.route.localGatewayMode = true;
      m_api.m_runtime.route.lastCloudRetryAttempt = millis();
      LOG_WARN("UPLOAD", F("Cloud unreachable. Switching to Gateway mode."));
      m_api.broadcastEncrypted(F("[SYSTEM] Cloud unreachable. Gateway mode active."));
    }
  }
}

bool ApiClientUploadRuntimeController::isHeapHealthy() {
  const ApiClientHealth::HeapBudget budget = ApiClientHealth::captureApiHeapBudget(m_ctx);
  if (!budget.healthy) {
    LOG_WARN("MEM", F("Low Mem - Skip. Block: %u, Total: %u"), budget.maxBlock, budget.freeHeap);
    return false;
  }
  return true;
}

void ApiClientUploadRuntimeController::processGatewayResult(const UploadResult& res) {
  if (m_api.m_runtime.queue.liveSnapshotInFlight) {
    m_api.m_runtime.queue.liveSnapshotInFlight = false;
    if (res.success) {
      LOG_INFO("GATEWAY", F("Live snapshot OK"));
      m_api.broadcastEncrypted(F("[GATEWAY] Live snapshot OK"));
    } else {
      LOG_WARN("GATEWAY", F("Live snapshot failed: %s"), res.message);
      char msg[80];
      int n = snprintf_P(msg, sizeof(msg), PSTR("[GATEWAY] Live snapshot fail (%d)"), res.httpCode);
      if (n > 0) {
        const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
        m_api.broadcastEncrypted(std::string_view(msg, len));
      }
    }
    return;
  }

  char sourceText[12];
  m_api.copyUploadSourceLabel(sourceText, sizeof(sourceText), m_api.m_runtime.route.loadedRecordSource);
  if (res.success) {
    LOG_INFO("GATEWAY", F("Notified: %s"), res.message);
    m_api.m_runtime.route.currentRecordSentToGateway = true;
    m_api.m_runtime.route.forceCloudAfterEdgeFailure = false;
    char msg[112];
    int n = snprintf_P(msg, sizeof(msg), PSTR("[GATEWAY] OK from %s (pending cloud sync)"), sourceText);
    if (n > 0) {
      const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
      m_api.broadcastEncrypted(std::string_view(msg, len));
    }
  } else {
    char msg[112];
    int n = snprintf_P(msg, sizeof(msg), PSTR("[GATEWAY] Fail from %s - retry cloud"), sourceText);
    if (n > 0) {
      const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
      m_api.broadcastEncrypted(std::string_view(msg, len));
    }
  }
  m_api.m_runtime.uploadState = ApiClient::UploadState::IDLE;

  if (!res.success) {
    m_api.clearCurrentRecordFlags();
    if (m_api.m_runtime.route.uploadMode == UploadMode::EDGE) {
      m_api.m_runtime.route.forceCloudAfterEdgeFailure = true;
    }
    trackUploadFailure();
  } else {
    m_api.m_runtime.consecutiveUploadFailures = 0;
  }
}

int ApiClientUploadRuntimeController::checkGatewayMode() {
  if (!m_api.m_transport.httpClient) {
    m_api.m_transport.httpClient.reset(new (std::nothrow) HTTPClient());
    if (!m_api.m_transport.httpClient) {
      return -1;
    }
  }
  m_api.m_transport.httpClient->setReuse(false);
  m_transport.httpClient->setTimeout(m_policy.edgeHttpTimeoutMs);

  char modeUrls[4][MAX_URL_LEN] = {{0}};
  char modePath[sizeof("/api/mode")];
  copy_trunc_P(modePath, sizeof(modePath), PSTR("/api/mode"));
  const size_t modeUrlCount = build_gateway_url_candidates(modeUrls, 4, m_api.m_deps.configManager, modePath);
  if (modeUrlCount == 0) {
    m_api.broadcastEncrypted(F("[MODE] Gateway poll failed (no URL)"));
    m_api.m_transport.httpClient.reset();
    return -1;
  }

  int detectedMode = -1;
  for (size_t i = 0; i < modeUrlCount; ++i) {
    const char* modeUrl = modeUrls[i];
    if (!modeUrl || modeUrl[0] == '\0') {
      continue;
    }
    char msg[96];
    int n = snprintf_P(msg, sizeof(msg), PSTR("[MODE] Gateway poll url=%s"), modeUrl);
    if (n > 0) {
      m_api.broadcastEncrypted(std::string_view(msg, static_cast<size_t>(n)));
    }

    if (!m_api.m_transport.httpClient->begin(m_api.m_transport.plainClient, modeUrl)) {
      continue;
    }

    int httpCode = m_api.m_transport.httpClient->GET();
    n = snprintf_P(msg, sizeof(msg), PSTR("[MODE] Gateway poll http=%d"), httpCode);
    if (n > 0) {
      m_api.broadcastEncrypted(std::string_view(msg, static_cast<size_t>(n)));
    }
    if (httpCode != 200) {
      m_api.m_transport.httpClient->end();
      continue;
    }

    char payload[96] = {0};
    WiFiClient& stream = REDACTED
    n = stream.readBytes(payload, sizeof(payload) - 1);
    if (n > 0) {
      payload[n] = '\0';
      const char* p = payload;
      const char* end = payload + n;
      while (p + 4 < end) {
        if (p[0] == 'm' && p[1] == 'o' && p[2] == 'd' && p[3] == 'e') {
          const char* c = p + 4;
          while (c < end && *c != ':') {
            ++c;
          }
          if (c < end && *c == ':') {
            ++c;
            while (c < end && (*c == ' ' || *c == '\t')) {
              ++c;
            }
            int val = 0;
            bool neg = false;
            if (c < end && *c == '-') {
              neg = true;
              ++c;
            }
            while (c < end && *c >= '0' && *c <= '9') {
              val = (val * 10) + (*c - '0');
              ++c;
            }
            val = neg ? -val : val;
            LOG_INFO("MODE", F("Gateway poll: %d (%s)"), val, ApiClient::gatewayModeLabel(val));
            n = snprintf_P(msg,
                           sizeof(msg),
                           PSTR("[MODE] Gateway mode=%d (%s)"),
                           val,
                           ApiClient::gatewayModeLabel(val));
            if (n > 0) {
              m_api.broadcastEncrypted(std::string_view(msg, static_cast<size_t>(n)));
            }
            m_api.m_transport.httpClient->end();
            detectedMode = val;
            break;
          }
        }
        ++p;
      }
    }
    if (detectedMode >= 0) {
      break;
    }
    m_api.m_transport.httpClient->end();
  }
  if (detectedMode < 0) {
    m_api.broadcastEncrypted(F("[MODE] Gateway poll failed"));
  }
  m_api.m_transport.httpClient.reset();
  return detectedMode;
}
