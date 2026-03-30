#include "api/ApiClient.QueueController.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <bearssl/bearssl_hmac.h>
#include <user_interface.h>

#include <algorithm>

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

#include "api/ApiClient.UploadShared.h"

using namespace ApiClientUploadShared;

// ApiClient.QueueEmergency.cpp - emergency queue persistence and backpressure orchestration

void ApiClientQueueController::applyQueuePopFailureCooldown(const AppConfig& cfg, const char* sourceTag) {
  if (m_api.m_runtime.queue.popFailStreak < 8) {
    m_api.m_runtime.queue.popFailStreak++;
  }
  const uint8_t shift = static_cast<uint8_t>(std::min<uint8_t>(m_api.m_runtime.queue.popFailStreak, 6));
  unsigned long cooldownMs = (1000UL << shift);
  cooldownMs = std::min<unsigned long>(cooldownMs, 60000UL);
  m_api.m_runtime.queue.popRetryAfter = millis() + cooldownMs;

  const unsigned long nextInterval = std::max<unsigned long>(cfg.CACHE_SEND_INTERVAL_MS, cooldownMs);
  m_api.m_runtime.cacheSendTimer.setInterval(nextInterval);
  m_api.m_runtime.cacheSendTimer.reset();

  char msg[144];
  char fallbackSource[12];
  if (!sourceTag || sourceTag[0] == '\0') {
    m_api.copyUploadSourceLabel(fallbackSource, sizeof(fallbackSource), ApiClient::UploadRecordSource::NONE);
    sourceTag = fallbackSource;
  }
  int n = snprintf_P(msg,
                     sizeof(msg),
                     PSTR("[QUEUE] Pop %s failed, cooldown %lu ms (streak=%u)"),
                     sourceTag,
                     cooldownMs,
                     static_cast<unsigned>(m_api.m_runtime.queue.popFailStreak));
  if (n > 0) {
    const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
    m_api.broadcastEncrypted(std::string_view(msg, len));
  }
}

void ApiClientQueueController::logEmergencyQueueState(ApiClient::EmergencyQueueReason reason) {
  const unsigned long nowMs = millis();
  const bool transition = (reason == ApiClient::EmergencyQueueReason::QUEUE_FULL ||
                           reason == ApiClient::EmergencyQueueReason::BACKPRESSURE_OFF ||
                           reason == ApiClient::EmergencyQueueReason::ENQUEUE);
  if (!transition && (nowMs - m_api.m_runtime.queue.lastEmergencyLogMs) < m_policy.emergencyLogThrottleMs) {
    return;
  }
  m_api.m_runtime.queue.lastEmergencyLogMs = nowMs;

  PGM_P reasonP = PSTR("state");
  switch (reason) {
    case ApiClient::EmergencyQueueReason::DRAINED:
      reasonP = PSTR("drained");
      break;
    case ApiClient::EmergencyQueueReason::BACKPRESSURE_OFF:
      reasonP = PSTR("backpressure-off");
      break;
    case ApiClient::EmergencyQueueReason::BACKPRESSURE_HOLD:
      reasonP = PSTR("backpressure-hold");
      break;
    case ApiClient::EmergencyQueueReason::ENQUEUE:
      reasonP = PSTR("enqueue");
      break;
    case ApiClient::EmergencyQueueReason::QUEUE_FULL:
      reasonP = PSTR("queue-full");
      break;
    case ApiClient::EmergencyQueueReason::STATE:
    default:
      break;
  }

  char reasonText[20];
  copy_trunc_P(reasonText, sizeof(reasonText), reasonP);

  LOG_WARN("EMERG",
           F("%s depth=%u/%u backpressure=%u rtc=%u lfs=%lu"),
           reasonText,
           static_cast<unsigned>(m_api.m_runtime.queue.emergencyCount),
           static_cast<unsigned>(ApiClient::kEmergencyQueueCapacity),
           static_cast<unsigned>(m_api.m_runtime.queue.emergencyBackpressure ? 1 : 0),
           static_cast<unsigned>(RtcManager::getCount()),
           static_cast<unsigned long>(m_api.m_deps.cacheManager.get_size()));

  char msg[168];
  int n = snprintf_P(msg,
                     sizeof(msg),
                     PSTR("[EMERG] %s | depth %u/%u | backpressure=%u | RTC %u/%u | LittleFS %lu/%lu B"),
                     reasonText,
                     static_cast<unsigned>(m_api.m_runtime.queue.emergencyCount),
                     static_cast<unsigned>(ApiClient::kEmergencyQueueCapacity),
                     static_cast<unsigned>(m_api.m_runtime.queue.emergencyBackpressure ? 1 : 0),
                     static_cast<unsigned>(RtcManager::getCount()),
                     static_cast<unsigned>(RTC_MAX_RECORDS),
                     static_cast<unsigned long>(m_api.m_deps.cacheManager.get_size()),
                     static_cast<unsigned long>(MAX_CACHE_DATA_SIZE));
  if (n > 0) {
    const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
    m_api.broadcastEncrypted(std::string_view(msg, len));
  }
}

bool ApiClientQueueController::persistEmergencyRecord(const ApiClient::EmergencyRecord& record, bool allowDirectSend) {
  const unsigned long nowMs = millis();

  const bool rtcHasCapacity = !RtcManager::isFull();
  if (rtcHasCapacity && m_api.appendEmergencyRecordToRtc(record, allowDirectSend)) {
    return true;
  }
  if (!rtcHasCapacity) {
    LOG_WARN("API", F("RTC still full before append, using LittleFS fallback to avoid overwrite"));
  }

  if (nowMs < m_api.m_runtime.queue.rtcFallbackFsRetryAfter) {
    const unsigned long waitMs = m_api.m_runtime.queue.rtcFallbackFsRetryAfter - nowMs;
    LOG_WARN("API", F("RTC append failed and LittleFS fallback cooling down (%lu ms)"), waitMs);
    return false;
  }

  if (m_api.appendEmergencyRecordToLittleFs(record, allowDirectSend)) {
    return true;
  }

  UploadResult directResult{};
  if (allowDirectSend && m_api.tryDirectSendEmergencyRecord(record, directResult)) {
    return true;
  }

  if (m_api.m_runtime.queue.rtcFallbackFsFailStreak < 8) {
    m_api.m_runtime.queue.rtcFallbackFsFailStreak++;
  }
  unsigned long cooldown = m_policy.littleFsFallbackCooldownBaseMs << m_api.m_runtime.queue.rtcFallbackFsFailStreak;
  cooldown = std::min<unsigned long>(cooldown, m_policy.littleFsFallbackCooldownMaxMs);
  m_api.m_runtime.queue.rtcFallbackFsRetryAfter = nowMs + cooldown;
  char directSendSuffix[12];
  copy_trunc_P(directSendSuffix,
               sizeof(directSendSuffix),
               allowDirectSend ? PSTR("/DirectSend") : PSTR(""));
  LOG_ERROR("API",
            F("Persist failed (RTC/LittleFS%s). Retry in %lu ms (code=%d, msg=%s)"),
            directSendSuffix,
            cooldown,
            directResult.httpCode,
            directResult.message);
  return false;
}

void ApiClientQueueController::drainEmergencyQueueToStorage(uint8_t maxRecords) {
  if (maxRecords == 0 || m_api.m_runtime.queue.emergencyCount == 0) {
    return;
  }

  uint8_t drained = 0;
  while (drained < maxRecords && m_api.m_runtime.queue.emergencyCount > 0) {
    ApiClient::EmergencyRecord front{};
    if (!m_api.peekEmergencyRecord(front)) {
      break;
    }
    if (!persistEmergencyRecord(front, false)) {
      break;
    }
    ApiClient::EmergencyRecord dropped{};
    if (!m_api.popEmergencyRecord(dropped)) {
      break;
    }
    drained++;
    ESP.wdtFeed();
    yield();
  }

  if (drained > 0) {
    logEmergencyQueueState(ApiClient::EmergencyQueueReason::DRAINED);
  }
  if (m_api.m_runtime.queue.emergencyBackpressure &&
      m_api.m_runtime.queue.emergencyCount < ApiClient::kEmergencyQueueCapacity) {
    m_api.m_runtime.queue.emergencyBackpressure = false;
    logEmergencyQueueState(ApiClient::EmergencyQueueReason::BACKPRESSURE_OFF);
  }
}

bool ApiClientQueueController::createAndCachePayload() {
  if (!m_api.ensureSharedBuffer()) {
    return false;
  }
  char* buf = m_api.sharedBuffer();
  const size_t buf_len = m_api.sharedBufferSize();
  if (!buf || buf_len == 0) {
    return false;
  }

  drainEmergencyQueueToStorage(2);
  if (m_api.m_runtime.queue.emergencyCount >= ApiClient::kEmergencyQueueCapacity) {
    m_api.m_runtime.queue.emergencyBackpressure = true;
    logEmergencyQueueState(ApiClient::EmergencyQueueReason::BACKPRESSURE_HOLD);
    m_api.resetSampleAccumulator();
    return true;
  }

  ApiClient::EmergencyRecord record{};
  m_api.populateEmergencyRecord(record);
  m_api.m_runtime.queue.pendingLiveSnapshot = record;
  m_api.m_runtime.queue.liveSnapshotPending = true;

  if (RtcManager::isFull()) {
    flushRtcToLittleFs();
  }

  if (!persistEmergencyRecord(record, true)) {
    if (m_api.enqueueEmergencyRecord(record)) {
      m_api.m_runtime.queue.emergencyBackpressure =
          (m_api.m_runtime.queue.emergencyCount >= ApiClient::kEmergencyQueueCapacity);
      logEmergencyQueueState(ApiClient::EmergencyQueueReason::ENQUEUE);
    } else {
      m_api.m_runtime.queue.emergencyBackpressure = true;
      LOG_ERROR("API", F("Emergency queue full, entering sampling backpressure"));
      logEmergencyQueueState(ApiClient::EmergencyQueueReason::QUEUE_FULL);
    }
  } else if (m_api.m_runtime.queue.emergencyBackpressure &&
             m_api.m_runtime.queue.emergencyCount < ApiClient::kEmergencyQueueCapacity) {
    m_api.m_runtime.queue.emergencyBackpressure = false;
    logEmergencyQueueState(ApiClient::EmergencyQueueReason::BACKPRESSURE_OFF);
  }

  m_api.resetSampleAccumulator();
  return true;
}
