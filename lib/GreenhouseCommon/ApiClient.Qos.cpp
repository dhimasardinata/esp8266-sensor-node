#include "ApiClient.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <bearssl/bearssl_hmac.h>
#include <user_interface.h>

#include <algorithm>
#include <cstring>
#include <new>

#include "CacheManager.h"  // Concrete type for CRTP
#include "CompileTimeJSON.h"
#include "ConfigManager.h"
#include "CryptoUtils.h"
#include "Logger.h"
#include "NtpClient.h"
#include "SensorManager.h"  // Concrete type for CRTP
#include "REDACTED"
#include "constants.h"
#include "node_config.h"
#include "sensor_data.h"
#include "utils.h"

// ApiClient.Qos.cpp - QoS testing

void ApiClient::requestQosUpload() {
  m_pendingQosTask = QosTaskType::UPLOAD;
}

void ApiClient::requestQosOta() {
  m_pendingQosTask = QosTaskType::OTA;
}

namespace {
  constexpr int kQosSamples = 5;

  static void copy_trunc(char* dst, size_t dst_len, const char* src) {
    if (!dst || dst_len == 0) {
      return;
    }
    if (!src) {
      dst[0] = '\0';
      return;
    }
    size_t n = strnlen(src, dst_len - 1);
    if (n > 0) {
      memcpy(dst, src, n);
    }
    dst[n] = '\0';
  }

  static size_t u32_to_dec(char* out, size_t out_len, uint32_t value) {
    if (!out || out_len == 0)
      return 0;
    char tmp[10];
    size_t n = 0;
    do {
      tmp[n++] = static_cast<char>('0' + (value % 10));
      value /= 10;
    } while (value != 0 && n < sizeof(tmp));
    size_t written = 0;
    while (n > 0 && written + 1 < out_len) {
      out[written++] = tmp[--n];
    }
    out[written] = '\0';
    return written;
  }

  static size_t append_literal(char* out, size_t out_len, size_t pos, const char* text) {
    if (!out || pos >= out_len || !text)
      return pos;
    size_t remaining = out_len - pos;
    if (remaining <= 1)
      return pos;
    size_t n = strnlen(text, remaining - 1);
    memcpy(out + pos, text, n);
    pos += n;
    out[pos] = '\0';
    return pos;
  }

  static size_t append_u32(char* out, size_t out_len, size_t pos, uint32_t value) {
    if (!out || pos >= out_len)
      return pos;
    size_t remaining = out_len - pos;
    if (remaining <= 1)
      return pos;
    size_t n = u32_to_dec(out + pos, remaining, value);
    pos += n;
    if (pos < out_len)
      out[pos] = '\0';
    return pos;
  }

  static size_t append_cstr(char* out, size_t out_len, size_t pos, const char* text) {
    return append_literal(out, out_len, pos, text);
  }

  static bool buildUrlWithNode(char* out, size_t out_len, const char* base, size_t base_len, uint32_t node_id) {
    if (!out || out_len == 0)
      return false;
    if (!base)
      base_len = 0;
    if (base_len > out_len - 1)
      base_len = out_len - 1;
    if (base_len > 0) {
      memcpy(out, base, base_len);
    }
    size_t pos = base_len;
    size_t digits = u32_to_dec(out + pos, out_len - pos, node_id);
    pos += digits;
    return digits > 0;
  }
}  // namespace

void ApiClient::handlePendingQosTask() {
  if (m_otaInProgress) {
    LOG_DEBUG("QoS", F("Deferred: OTA active"));
    return;
  }
  if (m_httpState != HttpState::IDLE || m_isSystemPaused) {
    LOG_WARN("QoS", F("Deferred: HTTP busy or system paused"));
    return;
  }
  const auto& cfg = m_configManager.getConfig();

  if (!m_qosActive) {
    if (m_pendingQosTask == QosTaskType::UPLOAD) {
      const char* dummyJson = "{\"qos_test\":1}";
      performQosTest("Data Upload API", m_configManager.getDataUploadUrl(), "POST", dummyJson);
      m_pendingQosTask = QosTaskType::NONE;
    } else if (m_pendingQosTask == QosTaskType::OTA) {
      const char* otaBase = REDACTED
      size_t base_len = strnlen(otaBase, MAX_URL_LEN);
      char urlBuf[160];
      if (!buildUrlWithNode(urlBuf, sizeof(urlBuf), otaBase, base_len, NODE_ID)) {
        LOG_ERROR("REDACTED", F("REDACTED"));
        m_pendingQosTask = QosTaskType::NONE;
        return;
      }
      performQosTest("REDACTED", urlBuf, "REDACTED", "REDACTED");
      m_pendingQosTask = QosTaskType::NONE;
    } else {
      return;
    }
    if (!m_qosActive) {
      return;  // Start failed (e.g., low heap).
    }
  }

  if (!m_qosBuffers) {
    m_qosActive = false;
    return;
  }
  if (millis() < m_qosNextAt) {
    return;
  }

  unsigned long duration = 0;
  if (executeQosSample(m_httpClient, m_qosBuffers->url, m_qosBuffers->method, m_qosBuffers->payload, cfg, duration)) {
    updateQosStats(duration, m_qosSuccessCount, m_qosTotalDuration, m_qosMinLat, m_qosMaxLat);
  } else {
    LOG_WARN("QoS", F("Req %d failed"), m_qosSampleIdx + 1);
  }

  m_qosSampleIdx++;
  if (m_qosSampleIdx >= kQosSamples) {
    reportQosResults(m_qosTargetName,
                     m_qosSuccessCount,
                     kQosSamples,
                     m_qosMinLat,
                     m_qosMaxLat,
                     m_qosTotalDuration);
    m_qosActive = false;
    m_qosBuffers.reset();
  } else {
    m_qosNextAt = millis() + 100;
  }
}

// =============================================================================
// performQosTest Helper Methods
// =============================================================================

void ApiClient::updateQosStats(unsigned long duration,
                               int& successCount,
                               unsigned long& totalDuration,
                               unsigned long& minLat,
                               unsigned long& maxLat) {
  successCount++;
  totalDuration += REDACTED
  if (duration < minLat)
    minLat = duration;
  if (duration > maxLat)
    maxLat = duration;
}

void ApiClient::reportQosResults(const char* targetName,
                                 int successCount,
                                 int samples,
                                 unsigned long minLat,
                                 unsigned long maxLat,
                                 unsigned long totalDuration) {
  uint32_t packetLoss = 0;
  if (samples > 0) {
    packetLoss =
        (static_cast<uint32_t>(samples - successCount) * 100U + (samples / 2)) / static_cast<uint32_t>(samples);
  }
  uint32_t avgLat =
      (successCount > 0) ? static_cast<uint32_t>((totalDuration + (successCount / 2)) / successCount) : REDACTED
  uint32_t jitter = (successCount > 0 && maxLat >= minLat) ? static_cast<uint32_t>(maxLat - minLat) : 0U;

  char report[256];
  report[0] = '\0';
  size_t pos = 0;
  pos = append_literal(report, sizeof(report), pos, "\n[REPORT] ");
  pos = append_cstr(report, sizeof(report), pos, targetName);
  pos = append_literal(report, sizeof(report), pos, "\n Requests    : ");
  pos = append_u32(report, sizeof(report), pos, static_cast<uint32_t>(successCount));
  pos = append_literal(report, sizeof(report), pos, "/");
  pos = append_u32(report, sizeof(report), pos, static_cast<uint32_t>(samples));
  pos = append_literal(report, sizeof(report), pos, " success\n Packet Loss : ");
  pos = append_u32(report, sizeof(report), pos, packetLoss);
  pos = append_literal(report, sizeof(report), pos, " %\n Latency (RT): Avg: ");
  pos = append_u32(report, sizeof(report), pos, avgLat);
  pos = append_literal(report, sizeof(report), pos, " ms | Min: ");
  pos = append_u32(report, sizeof(report), pos, static_cast<uint32_t>(minLat));
  pos = append_literal(report, sizeof(report), pos, " ms | Max: ");
  pos = append_u32(report, sizeof(report), pos, static_cast<uint32_t>(maxLat));
  pos = append_literal(report, sizeof(report), pos, " ms\n Jitter      : ");
  pos = append_u32(report, sizeof(report), pos, jitter);
  pos = append_literal(report, sizeof(report), pos, " ms\n-----------------------------");

  if (pos > 0) {
    broadcastEncrypted(std::string_view(report, pos));
  }
  LOG_INFO("QoS", F("Test Complete."));
}

// =============================================================================
// Main QoS Test Logic
// =============================================================================

void ApiClient::performQosTest(const char* targetName, const char* url, const char* method, const char* payload) {
  if (!targetName || !url || !method || !payload) {
    return;
  }
  if (m_qosActive) {
    return;
  }
  char msgBuf[128];
  msgBuf[0] = '\0';
  size_t pos = 0;
  pos = append_literal(msgBuf, sizeof(msgBuf), pos, "[QoS] Starting test for: ");
  pos = append_cstr(msgBuf, sizeof(msgBuf), pos, targetName);
  pos = append_literal(msgBuf, sizeof(msgBuf), pos, "...");
  if (pos > 0) {
    broadcastEncrypted(std::string_view(msgBuf, pos));
  }
  LOG_INFO("QoS", F("Testing %s (%s)"), targetName, url);

  // Verify sufficient heap availability.
  uint32_t freeBlock = ESP.getMaxFreeBlockSize();
  if (freeBlock < AppConstants::API_MIN_SAFE_BLOCK_SIZE) {
    LOG_ERROR("MEM", F("QoS Cancelled: Fragmentation too high! (Block: %u)"), freeBlock);
    broadcastEncrypted("[SYSTEM] QoS Cancelled: Low contiguous RAM. Try rebooting.");
    return;
  }

  m_qosBuffers.reset(new (std::nothrow) QosBuffers());
  if (!m_qosBuffers) {
    LOG_ERROR("QoS", F("Buffer alloc failed"));
    broadcastEncrypted("[SYSTEM] QoS Cancelled: Buffer alloc failed.");
    return;
  }

  copy_trunc(m_qosBuffers->url, sizeof(m_qosBuffers->url), url);
  copy_trunc(m_qosBuffers->method, sizeof(m_qosBuffers->method), method);
  copy_trunc(m_qosBuffers->payload, sizeof(m_qosBuffers->payload), payload);
  m_qosTargetName = targetName;
  m_qosSampleIdx = 0;
  m_qosSuccessCount = 0;
  m_qosTotalDuration = REDACTED
  m_qosMinLat = 0xFFFFFFFFu;
  m_qosMaxLat = 0;
  m_qosNextAt = millis();
  m_qosActive = true;

  m_httpClient.setReuse(true);
  m_httpClient.setTimeout(5000);
}
