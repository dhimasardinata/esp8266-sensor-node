#include "api/ApiClient.LifecycleController.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecureBearSSL.h>
#include <bearssl/bearssl_hmac.h>
#include <user_interface.h>

#include <algorithm>
#include <array>

#include "storage/CacheManager.h"  // Concrete type for CRTP
#include "system/ConfigManager.h"
#include "system/Logger.h"
#include "net/NtpClient.h"
#include "storage/RtcManager.h"
#include "sensor/SensorManager.h"  // Concrete type for CRTP
#include "REDACTED"
#include "config/constants.h"
#include "support/Utils.h"

#include "api/ApiClient.Health.h"
// ApiClient.Lifecycle.cpp - lifecycle facade and main loop controller

#include "api/ApiClient.CoreShared.h"

void ApiClientLifecycleController::init() {
  // Buffer sizes are configured once at boot to avoid heap churn.
}

void ApiClientLifecycleController::applyConfig(const AppConfig& config) {
  m_runtime.dataCreationTimer.setInterval(config.DATA_UPLOAD_INTERVAL_MS);
  m_runtime.sampleTimer.setInterval(config.SENSOR_SAMPLE_INTERVAL_MS);
  m_runtime.cacheSendTimer.setInterval(config.CACHE_SEND_INTERVAL_MS);
  m_runtime.cacheFlushTimer.setInterval(1800000);

  m_runtime.swWdtTimer.setInterval(config.SOFTWARE_WDT_TIMEOUT_MS);
  m_runtime.swWdtTimer.reset();

  const UplinkMode previousUplinkMode = m_runtime.route.uplinkMode;
  m_runtime.route.uplinkMode = config.UPLINK_MODE();
  if (m_runtime.route.uplinkMode != UplinkMode::AUTO || previousUplinkMode != m_runtime.route.uplinkMode) {
    m_runtime.route.forceRelayNextCloudAttempt = false;
    m_runtime.route.relayPinnedUntil = 0;
  }

  m_api.updateCloudTargetCache();
  m_runtime.route.cachedGatewayMode = -1;
  m_runtime.route.lastGatewayModeCheck = 0;
}

void ApiClientLifecycleController::handleTimerTasks() {
  ApiClientHealth::refreshRuntimeHealth(m_ctx);
  bool createdPayload = false;

  if (m_runtime.sampleTimer.hasElapsed()) {
    m_runtime.rssiSum += WiFi.RSSI();
    m_runtime.sampleCount++;
  }

  if (m_runtime.dataCreationTimer.hasElapsed()) {
    createdPayload = m_api.createAndCachePayload();
  }

  if (createdPayload || m_runtime.queue.liveSnapshotPending) {
    (void)m_api.trySendLiveSnapshotToGateway();
  }

  if (createdPayload &&
      m_transport.httpState == ApiClient::HttpState::IDLE &&
      m_runtime.uploadState == ApiClient::UploadState::IDLE &&
      !m_runtime.immediate.requested &&
      !m_runtime.queue.liveSnapshotInFlight) {
    m_api.releaseSharedBuffer();
  }

  if (m_transport.httpState == ApiClient::HttpState::IDLE &&
      m_runtime.uploadState == ApiClient::UploadState::IDLE &&
      m_runtime.cacheSendTimer.hasElapsed()) {
    if (m_deps.cacheManager.get_size() > 0 || RtcManager::getCount() > 0) {
      m_runtime.uploadState = ApiClient::UploadState::UPLOADING;
    }
  }

  if (m_runtime.uploadState == ApiClient::UploadState::UPLOADING) {
    m_api.handleUploadCycle();
  }

  if (m_runtime.cacheFlushTimer.hasElapsed()) {
    m_deps.cacheManager.flush();
  }
}

void ApiClientLifecycleController::checkSoftwareWdt() {
  if (m_runtime.lastApiSuccessMillis > 0 &&
      millis() - m_runtime.lastApiSuccessMillis > m_runtime.swWdtTimer.getInterval()) {
    LOG_ERROR("CRITICAL", F("Software WDT triggered. Rebooting!"));
    delay(1000);
    ESP.restart();
  }
}

void ApiClientLifecycleController::handle() {
  checkSoftwareWdt();
  ApiClientHealth::refreshRuntimeHealth(m_ctx);
  auto maybeRestoreTerminalWs = [&]() {
    if (!m_api.m_runtime.immediate.restoreWsAfter) {
      return;
    }
    if (m_api.m_runtime.immediate.requested || m_api.m_transport.httpState != ApiClient::HttpState::IDLE) {
      return;
    }
    if (m_api.m_deps.wifiManager.getState() != REDACTED
      return;
    }
    if (Utils::ws_set_enabled(true)) {
      LOG_INFO("WS", F("WS restored after immediate upload cycle"));
      m_api.m_runtime.immediate.restoreWsAfter = false;
    }
  };

  if (m_api.m_runtime.isSystemPaused) {
    return;
  }

  if (m_api.m_transport.httpState != ApiClient::HttpState::IDLE) {
    m_api.handleUploadStateMachine();

    if (m_api.m_transport.httpState == ApiClient::HttpState::COMPLETE) {
      m_api.m_transport.lastResult.success = (m_api.m_transport.lastResult.httpCode >= 200 && m_api.m_transport.lastResult.httpCode < 300);
      m_api.buildErrorMessage(m_api.m_transport.lastResult);

      if (m_api.m_runtime.route.targetIsEdge) {
        m_api.processGatewayResult(m_api.m_transport.lastResult);
      } else if (m_api.m_transport.lastResult.success) {
        m_api.handleSuccessfulUpload(m_api.m_transport.lastResult, m_api.m_deps.configManager.getConfig());
      } else {
        m_api.handleFailedUpload(m_api.m_transport.lastResult, m_api.m_deps.configManager.getConfig());
      }

      m_api.transitionState(ApiClient::HttpState::IDLE);
      m_api.releaseSharedBuffer();
      m_api.releaseTlsResources();
    } else if (m_api.m_transport.httpState == ApiClient::HttpState::FAILED) {
      if (m_api.m_transport.activeClient) {
        m_api.m_transport.activeClient->stop();
      }

      if (m_api.m_runtime.route.targetIsEdge) {
        m_api.processGatewayResult(m_api.m_transport.lastResult);
      } else {
        m_api.handleFailedUpload(m_api.m_transport.lastResult, m_api.m_deps.configManager.getConfig());
      }

      m_api.transitionState(ApiClient::HttpState::IDLE);
      m_api.releaseSharedBuffer();
      m_api.releaseTlsResources();
    }
  }

  if (m_api.m_qos.pendingTask != ApiClient::QosTaskType::NONE || m_api.m_qos.active) {
    m_api.handlePendingQosTask();
    return;
  }

  if (m_api.m_runtime.immediate.requested) {
    if (m_api.m_runtime.immediate.retryAt != 0 &&
        static_cast<int32_t>(millis() - m_api.m_runtime.immediate.retryAt) < 0) {
      return;
    }
    m_api.m_runtime.immediate.retryAt = 0;
    if (m_api.m_transport.httpState == ApiClient::HttpState::IDLE) {
      if (m_api.m_runtime.otaInProgress) {
        LOG_DEBUG("REDACTED", F("REDACTED"));
      } else {
        ApiClientHealth::refreshRuntimeHealth(m_ctx);
        if (m_health.wifiScanBusy) {
          LOG_DEBUG("REDACTED", F("REDACTED"));
          m_api.m_runtime.immediate.retryAt = millis() + 1000;
          return;
        }
        if (m_api.m_runtime.immediate.warmup > 0) {
          m_api.prepareTlsHeap();
          yield();
          m_api.m_runtime.immediate.warmup--;
          const unsigned long nowMs = millis();
          if (nowMs - m_api.m_runtime.immediate.lastDeferLog > 1000) {
            m_api.m_runtime.immediate.lastDeferLog = nowMs;
            LOG_INFO("API", F("Immediate upload pending (freeing buffers)"));
          }
          return;
        }
        const ApiClientHealth::HeapBudget tlsBudget = ApiClientHealth::captureTlsHeapBudget(m_ctx);
        if (!tlsBudget.healthy) {
          m_api.prepareTlsHeap();
          yield();
          const unsigned long nowMs = millis();
          if (nowMs - m_api.m_runtime.immediate.lastDeferLog > 1000) {
            m_api.m_runtime.immediate.lastDeferLog = nowMs;
            LOG_WARN("MEM",
                     F("Immediate upload deferred (low heap: %u, block %u, need %u/%u)"),
                     tlsBudget.freeHeap,
                     tlsBudget.maxBlock,
                     tlsBudget.minTotal,
                     tlsBudget.minBlock);
          }
          m_api.m_runtime.immediate.retryAt = millis() + 2000;
          return;
        }

        m_api.m_runtime.immediate.requested = false;
        LOG_INFO("API", F("Executing immediate upload..."));

        UploadResult result = m_api.performImmediateUpload();
        if (result.httpCode == ApiClient::kImmediateDeferred) {
          m_api.releaseSharedBuffer();
          m_api.m_runtime.immediate.retryAt = millis() + 2000;
          return;
        }

        char msg[80];
        if (result.success) {
          msg[0] = '\0';
          size_t pos = 0;
          pos = append_literal_P(msg, sizeof(msg), pos, PSTR("[SYSTEM] Upload OK (HTTP "));
          pos = append_i32(msg, sizeof(msg), pos, result.httpCode);
          pos = append_literal_P(msg, sizeof(msg), pos, PSTR(")"));
          if (pos > 0) {
            m_api.broadcastEncrypted(std::string_view(msg, pos));
          }
        } else {
          msg[0] = '\0';
          size_t pos = 0;
          pos = append_literal_P(msg, sizeof(msg), pos, PSTR("[SYSTEM] Fail: "));
          pos = append_cstr(msg, sizeof(msg), pos, result.message);
          pos = append_literal_P(msg, sizeof(msg), pos, PSTR(" ("));
          pos = append_i32(msg, sizeof(msg), pos, result.httpCode);
          pos = append_literal_P(msg, sizeof(msg), pos, PSTR(")"));
          if (pos > 0) {
            m_api.broadcastEncrypted(std::string_view(msg, pos));
          }
        }
        LOG_INFO("API", F("Immediate upload result: %s"), msg);
        m_api.releaseSharedBuffer();
        maybeRestoreTerminalWs();
        return;
      }
    } else {
      LOG_DEBUG("API", F("Immediate upload deferred (Busy)"));
      return;
    }
  }

  if (m_api.m_deps.wifiManager.getState() != REDACTED
    m_api.m_runtime.uploadState = ApiClient::UploadState::IDLE;
    return;
  }

  if (!m_api.m_runtime.otaInProgress &&
      !m_api.m_deps.ntpClient.isTimeSynced() &&
      m_api.m_transport.httpState == ApiClient::HttpState::IDLE) {
    m_api.tryNtpFallbackProbe();
  }

  handleTimerTasks();
  maybeRestoreTerminalWs();
}

void ApiClientLifecycleController::scheduleImmediateUpload() {
  if (m_api.createAndCachePayload()) {
    if (m_api.m_runtime.uploadState == ApiClient::UploadState::IDLE) {
      m_api.m_runtime.uploadState = ApiClient::UploadState::UPLOADING;
    }
    if (m_api.m_transport.httpState == ApiClient::HttpState::IDLE) {
      m_api.releaseSharedBuffer();
    }
  } else {
    LOG_ERROR("API", F("Failed to write to cache (Full/Error)"));
    m_api.broadcastEncrypted(F("[SYSTEM] Error: Failed to save data to cache!"));
  }
}

void ApiClientLifecycleController::requestImmediateUpload(bool restoreWsAfterUpload) {
  LOG_INFO("API", F("Immediate upload requested"));
  if (restoreWsAfterUpload) {
    m_api.m_runtime.immediate.restoreWsAfter = true;
    LOG_INFO("WS", F("WS restore scheduled after immediate upload"));
  }

  if (!m_api.createAndCachePayload()) {
    LOG_ERROR("API", F("Failed to create payload for immediate upload"));
    m_api.broadcastEncrypted(F("[SYSTEM] Error: Failed to create payload"));
    return;
  }

  m_api.m_runtime.immediate.requested = true;
  m_api.m_runtime.immediate.warmup = 1;
  m_api.m_runtime.immediate.retryAt = 0;
  m_api.m_runtime.immediate.pollReady = false;
  m_api.m_runtime.immediate.gatewayMode = -2;
  if (m_api.m_transport.httpState == ApiClient::HttpState::IDLE) {
    m_api.releaseSharedBuffer();
  }
}

ApiClient::ApiClient(AsyncWebSocket& ws,
                     NtpClient& ntpClient,
                     WifiManager& wifiManager,
                     SensorManager& sensorManager,
                     BearSSL::WiFiClientSecure& secureClient,
                     ConfigManager& configManager,
                     CacheManager& cacheManager,
                     const BearSSL::X509List* trustAnchors)
    : m_deps{ws, ntpClient, wifiManager, sensorManager, secureClient, configManager, cacheManager, trustAnchors},
      m_context{m_deps, m_runtime, m_transport, m_qos, m_resources, m_policy, m_health} {}

ApiClient::~ApiClient() = default;

void ApiClient::init() {
  ApiClientLifecycleController(*this).init();
}

void ApiClient::applyConfig(const AppConfig& config) {
  ApiClientLifecycleController(*this).applyConfig(config);
}

void ApiClient::handleTimerTasks() {
  ApiClientLifecycleController(*this).handleTimerTasks();
}

void ApiClient::checkSoftwareWdt() {
  ApiClientLifecycleController(*this).checkSoftwareWdt();
}

void ApiClient::handle() {
  ApiClientLifecycleController(*this).handle();
}

void ApiClient::scheduleImmediateUpload() {
  ApiClientLifecycleController(*this).scheduleImmediateUpload();
}

void ApiClient::requestImmediateUpload(bool restoreWsAfterUpload) {
  ApiClientLifecycleController(*this).requestImmediateUpload(restoreWsAfterUpload);
}

void ApiClient::requestImmediateUpload() {
  requestImmediateUpload(false);
}

unsigned long ApiClient::getLastSuccessMillis() const {
  return m_runtime.lastApiSuccessMillis;
}
