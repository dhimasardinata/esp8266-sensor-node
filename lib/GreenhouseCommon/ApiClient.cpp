#include "ApiClient.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecureBearSSL.h>
#include <bearssl/bearssl_hmac.h>
#include <user_interface.h>

#include <algorithm>
#include <array>
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
#include "root_ca_data.h"
#include "sensor_data.h"
#include "utils.h"

// ApiClient.cpp - lifecycle, control, and main loop
namespace {

  size_t u32_to_dec(char* out, size_t out_len, uint32_t value) {
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

  size_t append_literal(char* out, size_t out_len, size_t pos, const char* text) {
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

  size_t append_u32(char* out, size_t out_len, size_t pos, uint32_t value) {
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

  size_t append_i32(char* out, size_t out_len, size_t pos, int32_t value) {
    if (value < 0) {
      pos = append_literal(out, out_len, pos, "-");
      value = -value;
    }
    return append_u32(out, out_len, pos, static_cast<uint32_t>(value));
  }

  size_t append_cstr(char* out, size_t out_len, size_t pos, const char* text) {
    return append_literal(out, out_len, pos, text);
  }
}  // namespace

bool ApiClient::ensureSharedBuffer() {
  if (m_sharedBuffer) {
    return true;
  }
  std::unique_ptr<PayloadBuffer> buf(new (std::nothrow) PayloadBuffer());
  if (!buf) {
    LOG_WARN("MEM", F("Shared buffer alloc failed"));
    return false;
  }
  (*buf)[0] = '\0';
  m_sharedBuffer.swap(buf);
  return true;
}

void ApiClient::releaseSharedBuffer() {
  m_sharedBuffer.reset();
}

char* ApiClient::sharedBuffer() {
  return m_sharedBuffer ? m_sharedBuffer->data() : nullptr;
}

const char* ApiClient::sharedBuffer() const {
  return m_sharedBuffer ? m_sharedBuffer->data() : nullptr;
}

size_t ApiClient::sharedBufferSize() const {
  return m_sharedBuffer ? m_sharedBuffer->size() : 0;
}

bool ApiClient::ensureTrustAnchors() {
  if (m_trustAnchors) {
    return true;
  }
  if (m_localTrustAnchors) {
    return true;
  }
  std::unique_ptr<BearSSL::X509List> anchors(new (std::nothrow) BearSSL::X509List(ROOT_CA_PEM));
  if (!anchors) {
    LOG_WARN("MEM", F("Trust anchors alloc failed"));
    return false;
  }
  m_localTrustAnchors.swap(anchors);
  return true;
}

const BearSSL::X509List* ApiClient::activeTrustAnchors() const {
  if (m_trustAnchors) {
    return m_trustAnchors;
  }
  return m_localTrustAnchors.get();
}

void ApiClient::prepareTlsHeap() {
  // Release any lingering HTTP buffers before TLS handshake.
  m_httpClient.end();
  if (!m_qosActive) {
    m_qosBuffers.reset();
  }
  m_wifiManager.releaseScanCache();
  CryptoUtils::releaseWsCipher();
}

bool ApiClient::acquireTlsResources(bool allowInsecure) {
  prepareTlsHeap();
  yield();

  const uint32_t maxBlock = ESP.getMaxFreeBlockSize();
  const uint32_t totalFree = REDACTED
  uint32_t minBlock = AppConstants::TLS_MIN_SAFE_BLOCK_SIZE;
  uint32_t minTotal = REDACTED
  if (m_ws.count() > 0) {
    minBlock += 512;
    minTotal += REDACTED
  }
  if (maxBlock < minBlock || totalFree < minTotal) {
    LOG_WARN("MEM",
             F("TLS alloc skipped (low heap: %u, block %u, need %u/%u)"),
             totalFree,
             maxBlock,
             minTotal,
             minBlock);
    return false;
  }

  // Preferred: secure TLS with trust anchors.
  // Add guard margin to avoid BearSSL validator OOM on low/fragmented heap.
  constexpr uint32_t kSecureExtraBlock = 1024;
  constexpr uint32_t kSecureExtraTotal = REDACTED
  auto secure_guard_failed = [&]() {
    const uint32_t maxBlk = ESP.getMaxFreeBlockSize();
    const uint32_t total = REDACTED
    return (maxBlk < (AppConstants::TLS_MIN_SAFE_BLOCK_SIZE + kSecureExtraBlock) ||
            total < (AppConstants:REDACTED
  };

  auto configure_insecure = [&](bool logFallback) {
    static bool warned = false;
    if (logFallback && !warned) {
      LOG_WARN("SEC", F("API TLS fallback to insecure (low heap/frag for validator)"));
      broadcastEncrypted("[SEC] API TLS fallback to insecure (low heap/frag)");
      warned = true;
    }
    m_secureClient.stop();
    m_secureClient.setTimeout(15000);
    m_secureClient.setTrustAnchors(nullptr);
    m_localTrustAnchors.reset();
    m_secureClient.setInsecure();
    m_secureClient.setBufferSizes(AppConstants::TLS_RX_BUF_SIZE, AppConstants::TLS_TX_BUF_SIZE);
    m_tlsActive = true;
    m_tlsInsecure = true;
    return true;
  };

  if (m_tlsActive) {
    if (!m_tlsInsecure && secure_guard_failed()) {
      return configure_insecure(true);
    }
    return true;
  }

  m_secureClient.stop();
  m_secureClient.setTimeout(15000);

  // Insecure override (explicit config/flag).
  if (allowInsecure || m_configManager.getConfig().ALLOW_INSECURE_HTTPS()) {
    return configure_insecure(false);
  }

  if (!secure_guard_failed() && ensureTrustAnchors()) {
    const BearSSL::X509List* anchors = activeTrustAnchors();
    if (anchors && !secure_guard_failed()) {
      m_secureClient.setTrustAnchors(anchors);
      m_secureClient.setBufferSizes(AppConstants::TLS_RX_BUF_SIZE, AppConstants::TLS_TX_BUF_SIZE);
      if (!secure_guard_failed()) {
        m_tlsActive = true;
        m_tlsInsecure = false;
        return true;
      }
    }
  }

  return configure_insecure(true);
}

void ApiClient::releaseTlsResources() {
  if (!m_tlsActive) {
    return;
  }
  m_secureClient.stop();
  m_secureClient.setTrustAnchors(nullptr);
  m_secureClient.setInsecure();
  m_secureClient.setBufferSizes(AppConstants::TLS_RX_BUF_PORTAL, AppConstants::TLS_TX_BUF_PORTAL);
  m_localTrustAnchors.reset();
  m_tlsActive = false;
  m_tlsInsecure = false;
}

void ApiClient::setUploadMode(UploadMode mode) {
  m_uploadMode = mode;

  // Reset state when mode changes
  switch (mode) {
    case UploadMode::CLOUD:
      m_localGatewayMode = false;
      LOG_INFO("MODE", F("Upload mode set to CLOUD (forced)"));
      break;
    case UploadMode::EDGE:
      m_localGatewayMode = true;
      LOG_INFO("MODE", F("Upload mode set to EDGE (forced)"));
      break;
    case UploadMode::AUTO:
    default:
      // Maintain current state; automatic logic prevails.
      LOG_INFO("MODE", F("Upload mode set to AUTO (automatic fallback)"));
      break;
  }

  broadcastEncrypted(m_uploadMode == UploadMode::AUTO    ? "[MODE] Auto"
                     : m_uploadMode == UploadMode::CLOUD ? "[MODE] Cloud"
                                                         : "[MODE] Edge");
}


const char* ApiClient::getUploadModeString() const {
  switch (m_uploadMode) {
    case UploadMode::CLOUD:
      return "cloud";
    case UploadMode::EDGE:
      return "edge";
    case UploadMode::AUTO:
    default:
      return "auto";
  }
}

void ApiClient::broadcastUploadTarget(bool isEdge) {
  char url[128] = {0};
  if (isEdge) {
    buildLocalGatewayUrl(url, sizeof(url));
    if (url[0] == '\0') {
      (void)snprintf(url, sizeof(url), "%s", "gateway");
    }
  } else {
    if (m_cloudHost[0] == '\0' || m_cloudPath[0] == '\0') {
      updateCloudTargetCache();
    }
    const char* host = (m_cloudHost[0] != '\0') ? m_cloudHost : "example.com";
    const char* path = (m_cloudPath[0] != '\0') ? m_cloudPath : "/api/sensor";
    const char* scheme = "https";
    const char* raw = m_configManager.getDataUploadUrl();
    if (raw && strncmp(raw, "http://", 7) == 0) {
      scheme = "http";
    }
    size_t pos = 0;
    auto append = [&](const char* text, size_t len) {
      if (!text || len == 0 || pos + 1 >= sizeof(url)) {
        return;
      }
      size_t remaining = sizeof(url) - pos - 1;
      if (len > remaining) {
        len = remaining;
      }
      if (len > 0) {
        memcpy(url + pos, text, len);
        pos += len;
        url[pos] = '\0';
      }
    };
    append(scheme, strnlen(scheme, 8));
    append("://", 3);
    append(host, strnlen(host, sizeof(m_cloudHost)));
    append(path, strnlen(path, sizeof(m_cloudPath)));
  }

  char msg[160] = {0};
  (void)snprintf(msg, sizeof(msg), "[UPLOAD] target=%s", url);
  broadcastEncrypted(std::string_view(msg, strnlen(msg, sizeof(msg))));
  m_configManager.releaseStrings();
}


void ApiClient::broadcastEncrypted(const char* text) {
  if (!text)
    return;
  broadcastEncrypted(std::string_view(text, strlen(text)));
}

void ApiClient::broadcastEncrypted(std::string_view text) {
  if (text.empty() || m_ws.count() == 0)
    return;
  constexpr size_t kMaxText = CryptoUtils::MAX_PLAINTEXT_SIZE;
  static_assert(kMaxText <= CryptoUtils::MAX_PLAINTEXT_SIZE, "kMaxText exceeds plaintext limit");
  std::array<char, CryptoUtils::ENCRYPTION_BUFFER_SIZE> encScratch{};
  size_t offset = 0;
  while (offset < text.size()) {
    size_t chunk = text.size() - offset;
    if (chunk > kMaxText) {
      chunk = kMaxText;
    }
    size_t written =
        CryptoUtils::fast_serialize_encrypted_main(text.substr(offset, chunk), encScratch.data(), encScratch.size());
    if (written == 0) {
      break;
    }
    m_ws.textAll(encScratch.data(), written);
    offset += chunk;
  }
}

void ApiClient::pause() {
  if (!m_isSystemPaused) {
    LOG_INFO("REDACTED", F("REDACTED"));
    m_isSystemPaused = true;
    m_httpState = HttpState::IDLE;
    m_uploadState = UploadState::PAUSED;
    if (m_activeClient && m_activeClient->connected()) {
      m_activeClient->stop();
    }
    m_httpClient.end();
    releaseTlsResources();
  }
}


void ApiClient::resume() {
  if (m_isSystemPaused) {
    LOG_INFO("API", F("System Resumed."));
    m_isSystemPaused = false;
    m_uploadState = UploadState::IDLE;
    m_consecutiveUploadFailures = 0;
  }
}

void ApiClient::setTrustAnchors(const BearSSL::X509List* trustAnchors) {
  m_trustAnchors = trustAnchors;
  if (trustAnchors) {
    m_localTrustAnchors.reset();
  }
}

void ApiClient::setOtaInProgress(bool inProgress) {
  m_otaInProgress = REDACTED
}

bool ApiClient::isUploadActive() const {
  return (m_httpState != HttpState::IDLE) || m_qosActive;
}


ApiClient::ApiClient(AsyncWebSocket& ws,
                     NtpClient& ntpClient,
                     WifiManager& wifiManager,
                     SensorManager& sensorManager,
                     BearSSL::WiFiClientSecure& secureClient,
                     ConfigManager& configManager,
                     CacheManager& cacheManager,
                     const BearSSL::X509List* trustAnchors)
    : m_ws(ws),
      m_ntpClient(ntpClient),
      m_wifiManager(wifiManager),
      m_sensorManager(sensorManager),
      m_secureClient(secureClient),
      m_configManager(configManager),
      m_cacheManager(cacheManager),
      m_trustAnchors(trustAnchors) {}

ApiClient::~ApiClient() = default;


void ApiClient::init() {
  // Buffer sizes are configured once at boot to avoid heap churn.
}


void ApiClient::applyConfig(const AppConfig& config) {
  m_dataCreationTimer.setInterval(config.DATA_UPLOAD_INTERVAL_MS);
  m_sampleTimer.setInterval(config.SENSOR_SAMPLE_INTERVAL_MS);
  m_cacheSendTimer.setInterval(config.CACHE_SEND_INTERVAL_MS);

  // Flush cache header every 30 minutes to reduce flash wear (H-03)
  // 30 min = 1800000 ms
  m_cacheFlushTimer.setInterval(1800000);

  m_swWdtTimer.setInterval(config.SOFTWARE_WDT_TIMEOUT_MS);
  m_swWdtTimer.reset();

  updateCloudTargetCache();
  m_cachedGatewayMode = -1;
  m_lastGatewayModeCheck = 0;
}

// =============================================================================
// handle() Helper Methods
// =============================================================================


void ApiClient::handleTimerTasks() {
  if (m_sampleTimer.hasElapsed()) {
    m_rssiSum += WiFi.RSSI();
    m_sampleCount++;
  }

  if (m_uploadState == UploadState::IDLE && m_cacheSendTimer.hasElapsed()) {
    if (m_cacheManager.get_size() > 0) {
      m_uploadState = UploadState::UPLOADING;
    }
  }

  if (m_uploadState == UploadState::UPLOADING) {
    handleUploadCycle();
  }

  if (m_dataCreationTimer.hasElapsed()) {
    if (createAndCachePayload()) {
      if (m_httpState == HttpState::IDLE && m_uploadState == UploadState::IDLE && !m_immediateUploadRequested) {
        releaseSharedBuffer();
      }
    }
  }

  if (m_cacheFlushTimer.hasElapsed()) {
    m_cacheManager.flush();
  }
}


void ApiClient::checkSoftwareWdt() {
  if (m_lastApiSuccessMillis > 0 && millis() - m_lastApiSuccessMillis > m_swWdtTimer.getInterval()) {
    LOG_ERROR("CRITICAL", F("Software WDT triggered. Rebooting!"));
    delay(1000);
    ESP.restart();
  }
}

// =============================================================================
// Main Handle Loop
// =============================================================================

void ApiClient::handle() {
  // Check WDT *first* to ensure we reboot even if other conditions fail
  checkSoftwareWdt();

  if (m_isSystemPaused)
    return;

  // Handle State Machine
  if (m_httpState != HttpState::IDLE) {
    handleUploadStateMachine();

    // specific handling for completion
    if (m_httpState == HttpState::COMPLETE) {
      m_lastResult.success = (m_lastResult.httpCode >= 200 && m_lastResult.httpCode < 300);
      buildErrorMessage(m_lastResult, m_httpClient);
      // m_httpClient.end(); // Not used in raw mode

      // Callback Logic
      if (m_targetIsEdge)
        processGatewayResult(m_lastResult);
      else if (m_lastResult.success)
        handleSuccessfulUpload(m_lastResult, m_configManager.getConfig());
      else
        handleFailedUpload(m_lastResult, m_configManager.getConfig());

      transitionState(HttpState::IDLE);
      releaseSharedBuffer();
      releaseTlsResources();
    } else if (m_httpState == HttpState::FAILED) {
      // Force connection cleanup on failure
      if (m_activeClient) {
        m_activeClient->stop();
      }

      // Callback Logic for failure
      if (m_targetIsEdge)
        processGatewayResult(m_lastResult);
      else
        handleFailedUpload(m_lastResult, m_configManager.getConfig());

      transitionState(HttpState::IDLE);
      releaseSharedBuffer();
      releaseTlsResources();
    }
  }

  // Handle pending QoS task first

  if (m_pendingQosTask != QosTaskType::NONE || m_qosActive) {
    handlePendingQosTask();
    return;
  }

  // Handle immediate upload request (deferred from terminal command)
  if (m_immediateUploadRequested) {
    if (m_immediateRetryAt != 0 &&
        static_cast<int32_t>(millis() - m_immediateRetryAt) < 0) {
      return;
    }
    m_immediateRetryAt = 0;
    // RESOURCE GUARD: Only allow immediate upload if we are IDLE (to avoid stealing m_secureClient)
    if (m_httpState == HttpState::IDLE) {
      if (m_otaInProgress) {
        LOG_DEBUG("REDACTED", F("REDACTED"));
      } else {
        if (m_wifiManager.isScanBusy()) {
          LOG_DEBUG("REDACTED", F("REDACTED"));
          m_immediateRetryAt = millis() + 1000;
          return;
        }
        if (m_immediateWarmup > 0) {
          prepareTlsHeap();
          yield();
          m_immediateWarmup--;
          unsigned long nowMs = millis();
          if (nowMs - m_lastImmediateDeferLog > 1000) {
            m_lastImmediateDeferLog = nowMs;
            LOG_INFO("API", F("Immediate upload pending (freeing buffers)"));
          }
          return;
        }
        const uint32_t maxBlock = ESP.getMaxFreeBlockSize();
        const uint32_t totalFree = REDACTED
        uint32_t minBlock = AppConstants::TLS_MIN_SAFE_BLOCK_SIZE;
        uint32_t minTotal = REDACTED
        if (m_ws.count() > 0) {
          minBlock += 512;
          minTotal += REDACTED
        }
        if (maxBlock < minBlock || totalFree < minTotal) {
          prepareTlsHeap();
          yield();
          unsigned long nowMs = millis();
          if (nowMs - m_lastImmediateDeferLog > 1000) {
            m_lastImmediateDeferLog = nowMs;
            LOG_WARN("MEM",
                     F("Immediate upload deferred (low heap: %u, block %u, need %u/%u)"),
                     totalFree,
                     maxBlock,
                     minTotal,
                     minBlock);
          }
          m_immediateRetryAt = millis() + 2000;
          return;
        }

        m_immediateUploadRequested = false;
        LOG_INFO("API", F("Executing immediate upload..."));

        UploadResult result = performImmediateUpload();
        if (result.httpCode == kImmediateDeferred) {
          releaseSharedBuffer();
          m_immediateRetryAt = millis() + 2000;
          return;
        }

        char msg[80];
        if (result.success) {
          msg[0] = '\0';
          size_t pos = 0;
          pos = append_literal(msg, sizeof(msg), pos, "[SYSTEM] Upload OK (HTTP ");
          pos = append_i32(msg, sizeof(msg), pos, result.httpCode);
          pos = append_literal(msg, sizeof(msg), pos, ")");
          if (pos > 0) {
            broadcastEncrypted(std::string_view(msg, pos));
          }
        } else {
          msg[0] = '\0';
          size_t pos = 0;
          pos = append_literal(msg, sizeof(msg), pos, "[SYSTEM] Fail: ");
          pos = append_cstr(msg, sizeof(msg), pos, result.message);
          pos = append_literal(msg, sizeof(msg), pos, " (");
          pos = append_i32(msg, sizeof(msg), pos, result.httpCode);
          pos = append_literal(msg, sizeof(msg), pos, ")");
          if (pos > 0) {
            broadcastEncrypted(std::string_view(msg, pos));
          }
        }
        LOG_INFO("API", F("Immediate upload result: %s"), msg);
        releaseSharedBuffer();
        return;
      }
    } else {
      // Retry next loop
      LOG_DEBUG("API", F("Immediate upload deferred (Busy)"));
      return;
    }
  }

  // Require WiFi connection
  if (m_wifiManager.getState() != REDACTED
    m_uploadState = UploadState::IDLE;
    return;
  }

  // NTP fallback if not synced
  // RESOURCE GUARD: Only probe time if IDLE to avoid stealing m_secureClient
  if (!m_otaInProgress && !m_ntpClient.isTimeSynced() && m_httpState =REDACTED
    tryNtpFallbackProbe();
  }

  handleTimerTasks();
  // checkSoftwareWdt(); // Moved to top
}

// QoS Implementation
// =============================================================================


void ApiClient::scheduleImmediateUpload() {
  if (createAndCachePayload()) {
    if (m_uploadState == UploadState::IDLE) {
      m_uploadState = UploadState::UPLOADING;
    }
    if (m_httpState == HttpState::IDLE) {
      releaseSharedBuffer();
    }
  } else {
    // Report cache write failures.
    LOG_ERROR("API", F("Failed to write to cache (Full/Error)"));
    broadcastEncrypted("[SYSTEM] Error: Failed to save data to cache!");
  }
}


void ApiClient::requestImmediateUpload() {
  LOG_INFO("API", F("Immediate upload requested"));

  // Create payload first (this is fast, can run from any context)
  if (!createAndCachePayload()) {
    LOG_ERROR("API", F("Failed to create payload for immediate upload"));
    broadcastEncrypted("[SYSTEM] Error: Failed to create payload");
    return;
  }

  // Set flag - actual upload will run from main loop context
  m_immediateUploadRequested = true;
  m_immediateWarmup = 1;
  m_immediateRetryAt = 0;
  m_immediatePollReady = false;
  m_immediateGatewayMode = -2;
  if (m_httpState == HttpState::IDLE) {
    releaseSharedBuffer();
  }
}


unsigned long ApiClient::getLastSuccessMillis() const {
  return m_lastApiSuccessMillis;
}
