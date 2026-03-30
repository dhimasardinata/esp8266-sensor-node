#include "api/ApiClient.QosController.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <bearssl/bearssl_hmac.h>
#include <user_interface.h>

#include <algorithm>
#include <cstring>
#include <new>

#include "storage/CacheManager.h"  // Concrete type for CRTP
#include "support/CompileTimeJSON.h"
#include "system/ConfigManager.h"
#include "support/CryptoUtils.h"
#include "system/Logger.h"
#include "net/NtpClient.h"
#include "sensor/SensorManager.h"  // Concrete type for CRTP
#include "REDACTED"
#include "config/constants.h"
#include "generated/node_config.h"
#include "sensor/SensorData.h"
#include "support/Utils.h"
#include "api/ApiClient.Health.h"

// ApiClient.Qos.cpp - QoS testing

void ApiClientQosController::requestUpload() {
  m_qos.pendingTask = QosTaskType::UPLOAD;
}

void ApiClientQosController::requestOta() {
  m_qos.pendingTask = QosTaskType::OTA;
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

  static size_t append_literal_P(char* out, size_t out_len, size_t pos, PGM_P text) {
    if (!out || pos >= out_len || !text)
      return pos;
    size_t remaining = out_len - pos;
    if (remaining <= 1)
      return pos;
    size_t n = strlen_P(text);
    if (n > remaining - 1) {
      n = remaining - 1;
    }
    memcpy_P(out + pos, text, n);
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

void ApiClientQosController::handlePendingTask() {
  if (m_runtime.otaInProgress) {
    LOG_DEBUG("QoS", F("Deferred: OTA active"));
    return;
  }
  if (m_transport.httpState != HttpState::IDLE || m_runtime.isSystemPaused) {
    LOG_WARN("QoS", F("Deferred: HTTP busy or system paused"));
    return;
  }
  const auto& cfg = m_deps.configManager.getConfig();

  if (!m_qos.active) {
    if (m_qos.pendingTask == QosTaskType::UPLOAD) {
      const char* dummyJson = "{\"qos_test\":1}";
      performTest("Data Upload API", m_deps.configManager.getDataUploadUrl(), "POST", dummyJson);
      m_qos.pendingTask = QosTaskType::NONE;
    } else if (m_qos.pendingTask == QosTaskType::OTA) {
      const char* otaBase = REDACTED
      size_t base_len = strnlen(otaBase, MAX_URL_LEN);
      char urlBuf[160];
      if (!buildUrlWithNode(urlBuf, sizeof(urlBuf), otaBase, base_len, NODE_ID)) {
        LOG_ERROR("REDACTED", F("REDACTED"));
        m_qos.pendingTask = QosTaskType::NONE;
        m_deps.configManager.releaseStrings();
        return;
      }
      performTest("REDACTED", urlBuf, "REDACTED", "REDACTED");
      m_qos.pendingTask = QosTaskType::NONE;
    } else {
      return;
    }
    m_deps.configManager.releaseStrings();
    if (!m_qos.active) {
      return;  // Start failed (e.g., low heap).
    }
  }

  if (!m_qos.buffers) {
    m_qos.active = false;
    return;
  }
  if (millis() < m_qos.nextAt) {
    return;
  }

  if (!m_transport.httpClient) {
    LOG_ERROR("QoS", F("HTTP alloc missing; aborting"));
    m_api.broadcastEncrypted(F("[SYSTEM] QoS Cancelled: HTTP alloc failed."));
    m_qos.active = false;
    m_qos.buffers.reset();
    return;
  }
  unsigned long duration = 0;
  if (m_api.executeQosSample(*m_transport.httpClient,
                             m_qos.buffers->url,
                             m_qos.buffers->method,
                             m_qos.buffers->payload,
                             m_qos.usesOtaToken,
                             cfg,
                             duration)) {
    updateStats(duration, m_qos.successCount, m_qos.totalDuration, m_qos.minLat, m_qos.maxLat);
  } else {
    LOG_WARN("QoS", F("Req %d failed"), m_qos.sampleIdx + 1);
  }

  m_qos.sampleIdx++;
  if (m_qos.sampleIdx >= kQosSamples) {
    reportResults(m_qos.targetName,
                  m_qos.successCount,
                  kQosSamples,
                  m_qos.minLat,
                  m_qos.maxLat,
                  m_qos.totalDuration);
    m_qos.active = false;
    m_qos.buffers.reset();
    m_transport.httpClient.reset();
  } else {
    m_qos.nextAt = millis() + 100;
  }
}

// =============================================================================
// performQosTest Helper Methods
// =============================================================================

void ApiClientQosController::updateStats(unsigned long duration,
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

void ApiClientQosController::reportResults(const char* targetName,
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
  pos = append_literal_P(report, sizeof(report), pos, PSTR("\n[REPORT] "));
  pos = append_cstr(report, sizeof(report), pos, targetName);
  pos = append_literal_P(report, sizeof(report), pos, PSTR("\n Requests    : "));
  pos = append_u32(report, sizeof(report), pos, static_cast<uint32_t>(successCount));
  pos = append_literal_P(report, sizeof(report), pos, PSTR("/"));
  pos = append_u32(report, sizeof(report), pos, static_cast<uint32_t>(samples));
  pos = append_literal_P(report, sizeof(report), pos, PSTR(" success\n Packet Loss : "));
  pos = append_u32(report, sizeof(report), pos, packetLoss);
  pos = append_literal_P(report, sizeof(report), pos, PSTR(" %\n Latency (RT): Avg: "));
  pos = append_u32(report, sizeof(report), pos, avgLat);
  pos = append_literal_P(report, sizeof(report), pos, PSTR(" ms | Min: "));
  pos = append_u32(report, sizeof(report), pos, static_cast<uint32_t>(minLat));
  pos = append_literal_P(report, sizeof(report), pos, PSTR(" ms | Max: "));
  pos = append_u32(report, sizeof(report), pos, static_cast<uint32_t>(maxLat));
  pos = append_literal_P(report, sizeof(report), pos, PSTR(" ms\n Jitter      : "));
  pos = append_u32(report, sizeof(report), pos, jitter);
  pos = append_literal_P(report, sizeof(report), pos, PSTR(" ms\n-----------------------------"));

  if (pos > 0) {
    m_api.broadcastEncrypted(std::string_view(report, pos));
  }
  LOG_INFO("QoS", F("Test Complete."));
}

// =============================================================================
// Main QoS Test Logic
// =============================================================================

void ApiClientQosController::performTest(const char* targetName,
                                         const char* url,
                                         const char* method,
                                         const char* payload) {
  if (!targetName || !url || !method || !payload) {
    return;
  }
  if (m_qos.active) {
    return;
  }
  char msgBuf[128];
  msgBuf[0] = '\0';
  size_t pos = 0;
  pos = append_literal_P(msgBuf, sizeof(msgBuf), pos, PSTR("[QoS] Starting test for: "));
  pos = append_cstr(msgBuf, sizeof(msgBuf), pos, targetName);
  pos = append_literal_P(msgBuf, sizeof(msgBuf), pos, PSTR("..."));
  if (pos > 0) {
    m_api.broadcastEncrypted(std::string_view(msgBuf, pos));
  }
  LOG_INFO("QoS", F("Testing %s (%s)"), targetName, url);

  // Verify sufficient heap availability.
  const ApiClientHealth::HeapBudget apiBudget = ApiClientHealth::captureApiHeapBudget(m_ctx);
  if (!apiBudget.healthy) {
    LOG_ERROR("MEM", F("QoS Cancelled: Fragmentation too high! (Block: %u)"), apiBudget.maxBlock);
    m_api.broadcastEncrypted(F("[SYSTEM] QoS Cancelled: Low contiguous RAM. Try rebooting."));
    return;
  }

  m_qos.buffers.reset(new (std::nothrow) QosBuffers());
  if (!m_qos.buffers) {
    LOG_ERROR("QoS", F("Buffer alloc failed"));
    m_api.broadcastEncrypted(F("[SYSTEM] QoS Cancelled: Buffer alloc failed."));
    return;
  }

  if (!m_transport.httpClient) {
    m_transport.httpClient.reset(new (std::nothrow) HTTPClient());
    if (!m_transport.httpClient) {
      LOG_ERROR("QoS", F("HTTP alloc failed"));
      m_api.broadcastEncrypted(F("[SYSTEM] QoS Cancelled: HTTP alloc failed."));
      m_qos.buffers.reset();
      return;
    }
  }

  copy_trunc(m_qos.buffers->url, sizeof(m_qos.buffers->url), url);
  copy_trunc(m_qos.buffers->method, sizeof(m_qos.buffers->method), method);
  copy_trunc(m_qos.buffers->payload, sizeof(m_qos.buffers->payload), payload);
  m_qos.targetName = targetName;
  m_qos.usesOtaToken = REDACTED
  m_qos.sampleIdx = 0;
  m_qos.successCount = 0;
  m_qos.totalDuration = REDACTED
  m_qos.minLat = 0xFFFFFFFFu;
  m_qos.maxLat = 0;
  m_qos.nextAt = millis();
  m_qos.active = true;

  m_transport.httpClient->setReuse(true);
  m_transport.httpClient->setTimeout(m_policy.connectTimeoutMs);
}

void ApiClient::requestQosUpload() {
  ApiClientQosController(*this).requestUpload();
}

void ApiClient::requestQosOta() {
  ApiClientQosController(*this).requestOta();
}

void ApiClient::handlePendingQosTask() {
  ApiClientQosController(*this).handlePendingTask();
}

void ApiClient::updateQosStats(unsigned long duration,
                               int& successCount,
                               unsigned long& totalDuration,
                               unsigned long& minLat,
                               unsigned long& maxLat) {
  ApiClientQosController(*this).updateStats(duration, successCount, totalDuration, minLat, maxLat);
}

void ApiClient::reportQosResults(const char* targetName,
                                 int successCount,
                                 int samples,
                                 unsigned long minLat,
                                 unsigned long maxLat,
                                 unsigned long totalDuration) {
  ApiClientQosController(*this).reportResults(targetName, successCount, samples, minLat, maxLat, totalDuration);
}

void ApiClient::performQosTest(const char* targetName, const char* url, const char* method, const char* payload) {
  ApiClientQosController(*this).performTest(targetName, url, method, payload);
}
