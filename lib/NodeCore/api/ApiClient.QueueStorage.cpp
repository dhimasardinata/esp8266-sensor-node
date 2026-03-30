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

// ApiClient.QueueStorage.cpp - persisted record loading and RTC/LittleFS flushing

ApiClient::UploadRecordLoad ApiClientQueueController::loadRecordFromRtc(size_t& record_len) {
  record_len = 0;
  char* buf = m_api.sharedBuffer();
  const size_t buf_len = m_api.sharedBufferSize();
  if (!buf || buf_len == 0) {
    return ApiClient::UploadRecordLoad::FATAL;
  }

  RtcSensorRecord rec;
  const RtcReadStatus status = RtcManager::peekEx(rec);
  if (status == RtcReadStatus::CACHE_EMPTY) {
    return ApiClient::UploadRecordLoad::EMPTY;
  }
  if (status == RtcReadStatus::SCANNING) {
    return ApiClient::UploadRecordLoad::RETRY;
  }
  if (status == RtcReadStatus::CORRUPT_DATA) {
    if (m_api.m_runtime.route.loadedRecordSource == ApiClient::UploadRecordSource::RTC) {
      m_api.clearLoadedRecordContext();
    }
    m_api.broadcastEncrypted(F("[RTC] Corrupt slot dropped, retrying RTC read."));
    return ApiClient::UploadRecordLoad::RETRY;
  }
  if (status == RtcReadStatus::FILE_READ_ERROR) {
    return ApiClient::UploadRecordLoad::FATAL;
  }

  if (!build_payload_from_rtc_record(buf, buf_len, rec, record_len)) {
    return ApiClient::UploadRecordLoad::FATAL;
  }
  return ApiClient::UploadRecordLoad::READY;
}

ApiClient::UploadRecordLoad ApiClientQueueController::loadRecordFromLittleFs(size_t& record_len) {
  record_len = 0;
  char* buf = m_api.sharedBuffer();
  const size_t buf_len = m_api.sharedBufferSize();
  if (!buf || buf_len == 0) {
    return ApiClient::UploadRecordLoad::FATAL;
  }

  CacheReadError err = m_api.m_deps.cacheManager.read_one(buf, buf_len - 1, record_len);
  if (err == CacheReadError::NONE && record_len > 0) {
    buf[record_len] = '\0';
    return ApiClient::UploadRecordLoad::READY;
  }
  if (err == CacheReadError::CACHE_EMPTY || (err == CacheReadError::NONE && record_len == 0)) {
    return ApiClient::UploadRecordLoad::EMPTY;
  }
  if (err == CacheReadError::SCANNING) {
    return ApiClient::UploadRecordLoad::RETRY;
  }
  if (err == CacheReadError::CORRUPT_DATA) {
    m_api.broadcastEncrypted(F("[SYSTEM] LittleFS record corrupt, dropped."));
    (void)m_api.m_deps.cacheManager.pop_one();
    if (m_api.m_runtime.route.loadedRecordSource == ApiClient::UploadRecordSource::LITTLEFS) {
      m_api.clearLoadedRecordContext();
    }
    return ApiClient::UploadRecordLoad::RETRY;
  }
  return ApiClient::UploadRecordLoad::FATAL;
}

ApiClient::UploadRecordLoad ApiClientQueueController::loadRecordForUpload(size_t& record_len) {
  record_len = 0;

  if (m_api.m_runtime.route.loadedRecordSource == ApiClient::UploadRecordSource::RTC) {
    ApiClient::UploadRecordLoad locked = loadRecordFromRtc(record_len);
    if (locked == ApiClient::UploadRecordLoad::READY || locked == ApiClient::UploadRecordLoad::RETRY) {
      return locked;
    }
    m_api.m_runtime.route.loadedRecordSource = ApiClient::UploadRecordSource::NONE;
  } else if (m_api.m_runtime.route.loadedRecordSource == ApiClient::UploadRecordSource::LITTLEFS) {
    ApiClient::UploadRecordLoad locked = loadRecordFromLittleFs(record_len);
    if (locked == ApiClient::UploadRecordLoad::READY || locked == ApiClient::UploadRecordLoad::RETRY) {
      return locked;
    }
    m_api.m_runtime.route.loadedRecordSource = ApiClient::UploadRecordSource::NONE;
  }

  ApiClient::UploadRecordLoad rtcLoad = loadRecordFromRtc(record_len);
  if (rtcLoad == ApiClient::UploadRecordLoad::READY) {
    m_api.m_runtime.route.loadedRecordSource = ApiClient::UploadRecordSource::RTC;
    return ApiClient::UploadRecordLoad::READY;
  }

  ApiClient::UploadRecordLoad lfsLoad = loadRecordFromLittleFs(record_len);
  if (lfsLoad == ApiClient::UploadRecordLoad::READY) {
    m_api.m_runtime.route.loadedRecordSource = ApiClient::UploadRecordSource::LITTLEFS;
    return ApiClient::UploadRecordLoad::READY;
  }

  if (rtcLoad == ApiClient::UploadRecordLoad::FATAL || lfsLoad == ApiClient::UploadRecordLoad::FATAL) {
    return ApiClient::UploadRecordLoad::FATAL;
  }
  if (rtcLoad == ApiClient::UploadRecordLoad::RETRY || lfsLoad == ApiClient::UploadRecordLoad::RETRY) {
    return ApiClient::UploadRecordLoad::RETRY;
  }
  return ApiClient::UploadRecordLoad::EMPTY;
}

bool ApiClientQueueController::popLoadedRecord() {
  if (m_api.m_runtime.route.loadedRecordSource == ApiClient::UploadRecordSource::LITTLEFS) {
    for (uint8_t i = 0; i < 3; ++i) {
      if (m_api.m_deps.cacheManager.pop_one()) {
        return true;
      }
      ESP.wdtFeed();
      yield();
    }
    return false;
  }
  if (m_api.m_runtime.route.loadedRecordSource == ApiClient::UploadRecordSource::RTC) {
    RtcSensorRecord discarded;
    for (uint8_t i = 0; i < 3; ++i) {
      RtcReadStatus status = RtcManager::popEx(discarded);
      if (status == RtcReadStatus::NONE || status == RtcReadStatus::CACHE_EMPTY) {
        return true;
      }
      if (status == RtcReadStatus::SCANNING || status == RtcReadStatus::CORRUPT_DATA) {
        continue;
      }
      return false;
    }
  }
  return false;
}

void ApiClientQueueController::flushRtcToLittleFs() {
  LOG_WARN("RTC", F("[FLUSH]RTC Cache full! Bulk flushing %u records to LittleFS..."), RtcManager::getCount());
  {
    char msg[140];
    int n = snprintf_P(msg,
                       sizeof(msg),
                       PSTR("[CACHE] RTC->LittleFS flush started | RTC %u/%u | LittleFS %lu/%lu B"),
                       static_cast<unsigned>(RtcManager::getCount()),
                       static_cast<unsigned>(RTC_MAX_RECORDS),
                       static_cast<unsigned long>(m_api.m_deps.cacheManager.get_size()),
                       static_cast<unsigned long>(MAX_CACHE_DATA_SIZE));
    if (n > 0) {
      const size_t len = static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(msg) - 1)));
      m_api.broadcastEncrypted(std::string_view(msg, len));
    }
  }
  char* buf = m_api.sharedBuffer();
  const size_t buf_len = m_api.sharedBufferSize();
  if (!buf || buf_len == 0) {
    return;
  }

  const uint16_t maxAttempts = static_cast<uint16_t>(RTC_MAX_RECORDS * 4);
  uint16_t attempts = 0;
  RtcSensorRecord lastBuildFailure{};
  bool haveLastBuildFailure = false;
  while (attempts < maxAttempts) {
    attempts++;

    RtcSensorRecord rec;
    RtcReadStatus peekStatus = RtcManager::peekEx(rec);
    if (peekStatus == RtcReadStatus::CACHE_EMPTY) {
      LOG_INFO("RTC", F("[FLUSH]RTC empty, flush complete"));
      m_api.broadcastEncrypted(F("[CACHE] RTC->LittleFS flush complete (RTC empty)."));
      return;
    }
    if (peekStatus == RtcReadStatus::SCANNING) {
      LOG_WARN("RTC", F("[FLUSH]RTC recovery scanning in progress, retrying"));
      ESP.wdtFeed();
      continue;
    }
    if (peekStatus == RtcReadStatus::CORRUPT_DATA) {
      LOG_WARN("RTC", F("[FLUSH]RTC corrected corrupt front slot, retrying"));
      ESP.wdtFeed();
      continue;
    }
    if (peekStatus == RtcReadStatus::FILE_READ_ERROR) {
      LOG_ERROR("RTC", F("[FLUSH]RTC read/write error while peeking"));
      return;
    }

    size_t payloadLen = 0;
    if (!build_payload_from_rtc_record(buf, buf_len, rec, payloadLen)) {
      payloadLen = 0;
    }
    if (payloadLen == 0) {
      if (!haveLastBuildFailure || memcmp(&lastBuildFailure, &rec, sizeof(rec)) != 0) {
        lastBuildFailure = rec;
        haveLastBuildFailure = true;
        LOG_ERROR("API", F("[FLUSH]buildSensorPayload returned 0; retrying RTC front record once."));
        ESP.wdtFeed();
        continue;
      }

      LOG_ERROR("API", F("[FLUSH]Dropping RTC record that repeatedly fails payload serialization."));
      RtcSensorRecord discardedOnBuildFail;
      RtcReadStatus dropStatus = RtcManager::popEx(discardedOnBuildFail);
      if (dropStatus == RtcReadStatus::NONE) {
        haveLastBuildFailure = false;
        ESP.wdtFeed();
        continue;
      }
      if (dropStatus == RtcReadStatus::SCANNING || dropStatus == RtcReadStatus::CORRUPT_DATA) {
        ESP.wdtFeed();
        continue;
      }
      if (dropStatus == RtcReadStatus::CACHE_EMPTY) {
        LOG_INFO("RTC", F("[FLUSH]RTC empty after dropping invalid front record"));
        m_api.broadcastEncrypted(F("[CACHE] RTC->LittleFS flush complete (RTC empty)."));
        return;
      }
      LOG_ERROR("RTC", F("[FLUSH]RTC read/write error while dropping invalid front record"));
      return;
    }
    haveLastBuildFailure = false;
    if (!m_api.m_deps.cacheManager.write(buf, payloadLen)) {
      LOG_ERROR("API", F("LittleFS write failed during bulk flush!"));
      return;
    }

    RtcSensorRecord discarded;
    RtcReadStatus popStatus = RtcManager::popEx(discarded);
    if (popStatus == RtcReadStatus::NONE) {
      ESP.wdtFeed();
      continue;
    }
    if (popStatus == RtcReadStatus::SCANNING) {
      LOG_WARN("RTC", F("[FLUSH]RTC pop deferred: recovery scanning"));
      ESP.wdtFeed();
      continue;
    }
    if (popStatus == RtcReadStatus::CORRUPT_DATA) {
      LOG_WARN("RTC", F("[FLUSH]RTC pop hit corrupted data, retrying"));
      ESP.wdtFeed();
      continue;
    }
    if (popStatus == RtcReadStatus::CACHE_EMPTY) {
      LOG_INFO("RTC", F("[FLUSH]RTC empty after write, flush complete"));
      m_api.broadcastEncrypted(F("[CACHE] RTC->LittleFS flush complete (drained)."));
      return;
    }
    if (popStatus == RtcReadStatus::FILE_READ_ERROR) {
      LOG_ERROR("RTC", F("[FLUSH]RTC read/write error while popping"));
      return;
    }
  }

  LOG_ERROR("RTC", F("[FLUSH]Aborted by guard loop (attempts=%u)"), attempts);
}
