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

size_t ApiClientUploadRuntimeController::prepareEdgePayload(size_t rawLen) {
  if (!m_api.ensureSharedBuffer()) {
    return 0;
  }
  char* buf = m_api.sharedBuffer();
  const size_t buf_len = m_api.sharedBufferSize();
  if (!buf || buf_len == 0) {
    return 0;
  }
  char* closingBrace = strrchr(buf, '}');
  if (!closingBrace) {
    return 0;
  }

  char sendTimeValue[24];
  copy_default_datetime(sendTimeValue, sizeof(sendTimeValue));
  size_t sendTimeLen = sizeof("1970-01-01 00:00:00") - 1;
  (void)extract_recorded_at_value(buf, sendTimeValue, sizeof(sendTimeValue), sendTimeLen);

  if (!strip_recorded_at_field(buf, rawLen)) {
    return 0;
  }
  closingBrace = strrchr(buf, '}');
  if (!closingBrace) {
    return 0;
  }

  const int32_t nonActiveRssi = static_cast<int32_t>(resolve_nonactive_rssi(m_api.m_deps.wifiManager));

  char edgeOnlyFields[64];
  size_t fieldsPos = 0;
  if (!append_bytes_strict_P(edgeOnlyFields, sizeof(edgeOnlyFields), fieldsPos, PSTR(",\"rssi_nonactive\":"))) {
    return 0;
  }
  if (!append_i32_strict(edgeOnlyFields, sizeof(edgeOnlyFields), fieldsPos, nonActiveRssi)) {
    return 0;
  }
  if (!append_bytes_strict_P(edgeOnlyFields, sizeof(edgeOnlyFields), fieldsPos, PSTR(",\"send_time\":\""))) {
    return 0;
  }
  if (!append_bytes_strict(edgeOnlyFields, sizeof(edgeOnlyFields), fieldsPos, sendTimeValue, sendTimeLen)) {
    return 0;
  }
  if (!append_char_strict(edgeOnlyFields, sizeof(edgeOnlyFields), fieldsPos, '"')) {
    return 0;
  }
  if (!append_char_strict(edgeOnlyFields, sizeof(edgeOnlyFields), fieldsPos, '}')) {
    return 0;
  }

  const size_t insertPos = static_cast<size_t>(closingBrace - buf);
  if (insertPos + fieldsPos >= buf_len) {
    return 0;
  }
  memcpy(buf + insertPos, edgeOnlyFields, fieldsPos + 1);
  rawLen = insertPos + fieldsPos;

  std::array<char, CryptoUtils::ENCRYPTION_BUFFER_SIZE + 4> encBuffer{};
  strcpy_P(encBuffer.data(), PSTR("ENC:"));

  size_t encLen = CryptoUtils::fast_serialize_encrypted_main(
      std::string_view(buf, rawLen), encBuffer.data() + 4, encBuffer.size() - 4);

  if (encLen == 0) {
    return 0;
  }

  size_t totalLen = REDACTED
  if (totalLen >= REDACTED
    return 0;
  }

  memcpy(buf, encBuffer.data(), totalLen);
  buf[totalLen] = REDACTED
  return totalLen;
}

void ApiClientUploadRuntimeController::handleUploadCycle() {
  if (m_api.m_transport.httpState != ApiClient::HttpState::IDLE) {
    return;
  }

  if (m_api.m_runtime.otaInProgress) {
    m_api.m_runtime.cacheSendTimer.reset();
    return;
  }

  const bool ntpSynced = m_api.m_deps.ntpClient.isTimeSynced();
  const AppConfig& cfg = m_api.m_deps.configManager.getConfig();

  ApiClientHealth::refreshRuntimeHealth(m_ctx);
  if (m_health.wifiScanBusy) {
    m_api.m_runtime.cacheSendTimer.reset();
    return;
  }

  if (!isHeapHealthy()) {
    const uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    const uint32_t totalFree = REDACTED

    notifyLowMemory(maxBlock, totalFree);
    m_api.m_runtime.uploadState = ApiClient::UploadState::IDLE;
    m_api.m_runtime.cacheSendTimer.reset();
    m_api.m_runtime.lowMemCounter++;

    if (m_api.m_runtime.lowMemCounter > 10) {
      LOG_ERROR("MEM", F("Critical Memory Fragmentation persistent. Rebooting to self-heal."));
      delay(1000);
      ESP.restart();
    }
    return;
  }

  m_api.m_runtime.lowMemCounter = 0;

  if (!m_api.ensureSharedBuffer()) {
    m_api.m_runtime.uploadState = ApiClient::UploadState::IDLE;
    return;
  }
  char* buf = m_api.sharedBuffer();
  const size_t buf_len = m_api.sharedBufferSize();
  if (!buf || buf_len == 0) {
    m_api.m_runtime.uploadState = ApiClient::UploadState::IDLE;
    return;
  }

  if (m_api.m_runtime.queue.popRetryAfter != 0 &&
      static_cast<int32_t>(millis() - m_api.m_runtime.queue.popRetryAfter) < 0) {
    m_api.m_runtime.uploadState = ApiClient::UploadState::IDLE;
    m_api.releaseSharedBuffer();
    return;
  }

  if (m_api.recoverPendingQueuePop(cfg, false, nullptr)) {
    m_api.m_runtime.uploadState = ApiClient::UploadState::IDLE;
    m_api.releaseSharedBuffer();
    return;
  }

  ESP.wdtFeed();
  size_t record_len = 0;
  ApiClient::UploadRecordLoad loadStatus = m_api.loadRecordForUpload(record_len);
  if (loadStatus == ApiClient::UploadRecordLoad::EMPTY) {
    m_api.clearLoadedRecordContext();
    m_api.resetQueuedUploadCycle(false);
    return;
  }
  if (loadStatus == ApiClient::UploadRecordLoad::RETRY) {
    m_api.resetQueuedUploadCycle(false);
    return;
  }
  if (loadStatus == ApiClient::UploadRecordLoad::FATAL) {
    m_api.resetQueuedUploadCycle(true);
    return;
  }

  buf[record_len] = '\0';

  bool isTargetEdge = false;
  const ApiClient::QueuedUploadTargetDecision targetDecision =
      m_api.resolveQueuedUploadTarget(isTargetEdge);
  if (targetDecision == ApiClient::QueuedUploadTargetDecision::WAIT) {
    m_api.resetQueuedUploadCycle(true);
    return;
  }
  if (targetDecision == ApiClient::QueuedUploadTargetDecision::HOLD) {
    m_api.clearCurrentRecordFlags();
    m_api.resetQueuedUploadCycle(true);
    return;
  }

  if (!ntpSynced && !isTargetEdge) {
    static unsigned long lastNtpWarn = 0;
    if (millis() - lastNtpWarn > 60000UL) {
      LOG_WARN("TIME", F("NTP not synced; cloud upload deferred"));
      lastNtpWarn = millis();
    }
    m_api.resetQueuedUploadCycle(true);
    return;
  }

  (void)buf_len;
  (void)dispatchQueuedUploadRecord(record_len, isTargetEdge);
}

bool ApiClientUploadRuntimeController::dispatchQueuedUploadRecord(size_t record_len, bool isTargetEdge) {
  char* buf = m_api.sharedBuffer();
  if (!buf) {
    m_api.resetQueuedUploadCycle(true);
    return false;
  }

  if (isTargetEdge) {
    const size_t encLen = prepareEdgePayload(record_len);
    if (encLen == 0) {
      LOG_ERROR("API", F("Encryption failed. Skipping."));
      m_api.resetQueuedUploadCycle(true);
      return false;
    }
    m_api.broadcastUploadDispatch(false, true, record_len);
    m_api.startUpload(buf, encLen, true);
  } else {
    m_api.broadcastUploadDispatch(false, false, record_len);
    m_api.startUpload(buf, record_len, false);
  }

  if (m_api.m_transport.httpState == ApiClient::HttpState::IDLE) {
    LOG_WARN("API", F("Queued upload did not start"));
    m_api.resetQueuedUploadCycle(true);
    return false;
  }
  return true;
}

bool ApiClientUploadRuntimeController::trySendLiveSnapshotToGateway() {
  bool gatewayLiveMode = false;
  if (m_api.m_runtime.route.uploadMode == UploadMode::EDGE) {
    gatewayLiveMode = true;
  } else if (m_api.m_runtime.route.uploadMode == UploadMode::AUTO) {
    gatewayLiveMode = m_api.m_runtime.route.localGatewayMode || m_api.m_runtime.route.cachedGatewayMode == 1;
  }

  if (!gatewayLiveMode) {
    m_api.m_runtime.queue.liveSnapshotPending = false;
    return false;
  }
  if (!m_api.m_runtime.queue.liveSnapshotPending || m_api.m_runtime.queue.liveSnapshotInFlight) {
    return false;
  }
  if (m_api.m_runtime.isSystemPaused || m_api.m_runtime.otaInProgress || m_api.m_runtime.immediate.requested) {
    return false;
  }
  if (m_api.m_transport.httpState != ApiClient::HttpState::IDLE ||
      m_api.m_runtime.uploadState != ApiClient::UploadState::IDLE) {
    return false;
  }
  ApiClientHealth::refreshRuntimeHealth(m_ctx);
  if (m_health.wifiScanBusy || !isHeapHealthy()) {
    return false;
  }
  if (!m_api.ensureSharedBuffer()) {
    return false;
  }

  char* buf = m_api.sharedBuffer();
  const size_t buf_len = m_api.sharedBufferSize();
  if (!buf || buf_len == 0) {
    return false;
  }

  size_t record_len = 0;
  if (!m_api.buildPayloadFromEmergencyRecord(m_api.m_runtime.queue.pendingLiveSnapshot, buf, buf_len, record_len)) {
    return false;
  }

  const size_t encLen = prepareEdgePayload(record_len);
  if (encLen == 0) {
    return false;
  }

  char msg[96];
  int n = snprintf_P(msg, sizeof(msg), PSTR("[UPLOAD] EDGE live snapshot (%u B)"), static_cast<unsigned>(record_len));
  if (n > 0) {
    const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
    m_api.broadcastEncrypted(std::string_view(msg, len));
  }

  m_api.m_runtime.queue.liveSnapshotPending = false;
  m_api.m_runtime.queue.liveSnapshotInFlight = true;
  m_api.startUpload(buf, encLen, true);
  if (m_api.m_transport.httpState == ApiClient::HttpState::IDLE) {
    m_api.m_runtime.queue.liveSnapshotInFlight = false;
    m_api.m_runtime.queue.liveSnapshotPending = true;
    return false;
  }
  return true;
}
