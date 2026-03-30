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
#include "storage/RtcManager.h"
#include "sensor/SensorManager.h"
#include "REDACTED"
#include "config/constants.h"
#include "generated/node_config.h"
#include "sensor/SensorData.h"
#include "support/Utils.h"

#include "api/ApiClient.UploadShared.h"

using namespace ApiClientUploadShared;

// ApiClient.UploadRecords.cpp - emergency record creation and persistence helpers

void ApiClientUploadController::populateEmergencyRecord(ApiClient::EmergencyRecord& outRecord) {
  const auto& cfg = m_api.m_deps.configManager.getConfig();
  const auto effective = SensorNormalization::makeEffectiveSensorSnapshot(m_api.m_deps.sensorManager.getTemp(),
                                                                          m_api.m_deps.sensorManager.getHumidity(),
                                                                          m_api.m_deps.sensorManager.getLight(),
                                                                          cfg.TEMP_OFFSET,
                                                                          cfg.HUMIDITY_OFFSET,
                                                                          cfg.LUX_SCALING_FACTOR);

  int32_t temp10 = 0;
  if (effective.temperature.isValid) {
    temp10 = SensorNormalization::clampTemperatureTenths(
        SensorNormalization::roundToNearestInt(effective.temperature.effectiveValue * 10.0f));
  }

  int32_t hum10 = 0;
  if (effective.humidity.isValid) {
    hum10 = SensorNormalization::clampHumidityTenths(
        SensorNormalization::roundToNearestInt(effective.humidity.effectiveValue * 10.0f));
  }

  uint16_t luxVal = 0;
  if (effective.light.isValid) {
    luxVal = static_cast<uint16_t>(
        SensorNormalization::clampLightUInt(static_cast<uint32_t>(effective.light.effectiveValue)));
  }

  const long rssiVal = (m_api.m_runtime.sampleCount > 0) ? (m_api.m_runtime.rssiSum / m_api.m_runtime.sampleCount)
                                                         : WiFi.RSSI();
  const time_t now = time(nullptr);

  outRecord = {};
  outRecord.timestamp = (now > NTP_VALID_TIMESTAMP_THRESHOLD) ? static_cast<uint32_t>(now) : 0U;
  outRecord.temp10 = static_cast<int16_t>(temp10);
  outRecord.hum10 = static_cast<int16_t>(hum10);
  outRecord.lux = luxVal;
  outRecord.rssi = static_cast<int16_t>(rssiVal);
}

bool ApiClientUploadController::buildPayloadFromEmergencyRecord(const ApiClient::EmergencyRecord& record,
                                                                char* out,
                                                                size_t out_len,
                                                                size_t& payload_len) const {
  return build_payload_from_record_fields(out,
                                          out_len,
                                          record.timestamp,
                                          static_cast<int32_t>(record.temp10),
                                          static_cast<int32_t>(record.hum10),
                                          static_cast<uint32_t>(record.lux),
                                          static_cast<int32_t>(record.rssi),
                                          payload_len);
}

bool ApiClientUploadController::appendEmergencyRecordToRtc(const ApiClient::EmergencyRecord& record, bool announce) {
  static constexpr uint8_t kRtcAppendRetries = 3;

  for (uint8_t rtcAttempt = 1; rtcAttempt <= kRtcAppendRetries; ++rtcAttempt) {
    if (RtcManager::append(record.timestamp, record.temp10, record.hum10, record.lux, record.rssi)) {
      resetRtcFallbackRecovery();
      if (announce) {
        char msg[128];
        int n = snprintf_P(msg,
                           sizeof(msg),
                           PSTR("[CACHE] Stored in RTC | RTC %u/%u | LittleFS %lu/%lu B"),
                           static_cast<unsigned>(RtcManager::getCount()),
                           static_cast<unsigned>(RTC_MAX_RECORDS),
                           static_cast<unsigned long>(m_api.m_deps.cacheManager.get_size()),
                           static_cast<unsigned long>(MAX_CACHE_DATA_SIZE));
        if (n > 0) {
          const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
          m_api.broadcastEncrypted(std::string_view(msg, len));
        }
      }
      return true;
    }
    LOG_WARN("API",
             F("RTC append attempt %u/%u failed"),
             static_cast<unsigned>(rtcAttempt),
             static_cast<unsigned>(kRtcAppendRetries));
    ESP.wdtFeed();
  }
  return false;
}

bool ApiClientUploadController::appendEmergencyRecordToLittleFs(const ApiClient::EmergencyRecord& record,
                                                                bool announce) {
  static constexpr uint8_t kFallbackFsWriteRetries = 2;

  if (!m_api.ensureSharedBuffer()) {
    return false;
  }
  char* buf = m_api.sharedBuffer();
  const size_t buf_len = m_api.sharedBufferSize();
  if (!buf || buf_len == 0) {
    return false;
  }

  size_t payloadLen = 0;
  if (!buildPayloadFromEmergencyRecord(record, buf, buf_len, payloadLen)) {
    LOG_ERROR("API", F("Fallback payload build failed"));
    return false;
  }

  for (uint8_t fsAttempt = 1; fsAttempt <= kFallbackFsWriteRetries; ++fsAttempt) {
    if (m_api.m_deps.cacheManager.write(buf, payloadLen)) {
      resetRtcFallbackRecovery();
      if (announce) {
        LOG_WARN("API", F("RTC append failed, record stored directly to LittleFS fallback"));
        char msg[128];
        int n = snprintf_P(msg,
                           sizeof(msg),
                           PSTR("[CACHE] RTC write failed, stored in LittleFS | RTC %u/%u | LittleFS %lu/%lu B"),
                           static_cast<unsigned>(RtcManager::getCount()),
                           static_cast<unsigned>(RTC_MAX_RECORDS),
                           static_cast<unsigned long>(m_api.m_deps.cacheManager.get_size()),
                           static_cast<unsigned long>(MAX_CACHE_DATA_SIZE));
        if (n > 0) {
          const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
          m_api.broadcastEncrypted(std::string_view(msg, len));
        }
      }
      return true;
    }
    LOG_WARN("API",
             F("Fallback LittleFS write attempt %u/%u failed"),
             static_cast<unsigned>(fsAttempt),
             static_cast<unsigned>(kFallbackFsWriteRetries));
    ESP.wdtFeed();
  }

  return false;
}

bool ApiClientUploadController::tryDirectSendEmergencyRecord(const ApiClient::EmergencyRecord& record,
                                                             UploadResult& result) {
  result = {};
  result.httpCode = HTTPC_ERROR_CONNECTION_FAILED;
  copy_trunc_P(result.message, sizeof(result.message), PSTR("Emergency send failed"));

  if (!m_api.ensureSharedBuffer()) {
    return false;
  }
  char* buf = m_api.sharedBuffer();
  const size_t buf_len = m_api.sharedBufferSize();
  if (!buf || buf_len == 0) {
    return false;
  }

  size_t payloadLen = 0;
  if (!buildPayloadFromEmergencyRecord(record, buf, buf_len, payloadLen)) {
    return false;
  }

  bool directToEdge = false;
  if (m_api.m_runtime.route.uploadMode == UploadMode::EDGE) {
    directToEdge = true;
  } else if (m_api.m_runtime.route.uploadMode == UploadMode::AUTO) {
    directToEdge = (m_api.m_runtime.route.cachedGatewayMode == 1) || m_api.m_runtime.route.localGatewayMode;
  }

  if (directToEdge) {
    const size_t edgeLen = m_api.prepareEdgePayload(payloadLen);
    if (edgeLen > 0) {
      result = m_api.performLocalGatewayUpload(buf, edgeLen);
    } else {
      result.httpCode = HTTPC_ERROR_TOO_LESS_RAM;
      snprintf_P(result.message, sizeof(result.message), PSTR("Edge encrypt fail"));
    }
  } else {
    result = m_api.performSingleUpload(buf, payloadLen, false);
  }
  m_api.m_transport.httpClient.reset();

  if (!result.success) {
    return false;
  }

  resetRtcFallbackRecovery();
  m_api.m_runtime.lastApiSuccessMillis = millis();
  m_api.m_runtime.consecutiveUploadFailures = 0;
  char directTarget[6];
  copy_trunc_P(directTarget, sizeof(directTarget), directToEdge ? PSTR("EDGE") : PSTR("CLOUD"));
  LOG_WARN("API",
           F("RTC+LittleFS failed, emergency direct send succeeded via %s (HTTP %d)"),
           directTarget,
           result.httpCode);
  return true;
}
