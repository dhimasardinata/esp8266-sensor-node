#include "ApiClient.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <math.h>
#include <user_interface.h>

#include <algorithm>
#include <memory>

#include "ConfigManager.h"
#include "CryptoUtils.h"
#include "ICacheManager.h"
#include "ISensorManager.h"
#include "Logger.h"
#include "NtpClient.h"
#include "WifiManager.h"
#include "constants.h"
#include "node_config.h"
#include "root_ca_data.h"
#include "sensor_data.h"
#include "utils.h"

// =============================================================================
// Helper Functions to Reduce Cyclomatic Complexity
// =============================================================================

void ApiClient::tryNtpFallbackProbe() {
  // If NTP is stuck for > 60 seconds, try HTTP Probe for Date header
  if (millis() > 60000 && millis() - m_lastTimeProbe > 60000) {
    m_lastTimeProbe = millis();
    LOG_WARN("TIME", F("NTP stuck. Probing HTTP server for 'Date' header..."));
    performSingleUpload("{}", 2);
  }
}

void ApiClient::setupHttpHeaders(HTTPClient& http, const AppConfig& cfg) {
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader(F("Connection"), F("close"));
  http.addHeader(F("Accept"), F("application/json"));
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("User-Agent"),
                 F("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                   "Chrome/120.0.0.0 Safari/537.36"));

  char authBuffer[MAX_TOKEN_LEN + 20];
  snprintf(authBuffer, sizeof(authBuffer), "Bearer %s", cfg.AUTH_TOKEN.data());
  http.addHeader(F("Authorization"), authBuffer);
}

void ApiClient::extractTimeFromResponse(HTTPClient& http) {
  if (m_ntpClient.isTimeSynced())
    return;

  String dateHeader = http.header("Date");
  if (dateHeader.length() > 0) {
    time_t serverTime = Utils::parse_http_date(dateHeader.c_str());
    if (serverTime > 0) {
      m_ntpClient.setManualTime(serverTime);
    }
  }
}

void ApiClient::buildErrorMessage(UploadResult& result, HTTPClient& http) {
  if (result.success) {
    snprintf(result.message, sizeof(result.message), "OK");
    return;
  }

  if (result.httpCode < 0) {
    snprintf(result.message, sizeof(result.message), "%s", http.errorToString(result.httpCode).c_str());
    return;
  }

  // Data-driven HTTP error lookup table
  static constexpr struct {
    int code;
    const char* reason;
  } httpErrors[] = {{400, "Bad Request"},
                    {401, "Unauthorized"},
                    {403, "Forbidden"},
                    {404, "Not Found"},
                    {419, "Session Expired"},
                    {422, "Unprocessable"},
                    {429, "Too Many Requests"},
                    {500, "Server Error"}};

  const char* reason = "Error";
  for (const auto& err : httpErrors) {
    if (result.httpCode == err.code) {
      reason = err.reason;
      break;
    }
  }
  snprintf(result.message, sizeof(result.message), "HTTP %d (%s)", result.httpCode, reason);
}

// =============================================================================
// Local Gateway Fallback Methods
// =============================================================================

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
      // Keep current gateway state, let automatic logic handle it
      LOG_INFO("MODE", F("Upload mode set to AUTO (automatic fallback)"));
      break;
  }
  
  broadcastEncrypted(m_uploadMode == UploadMode::AUTO ? "[MODE] Auto" :
                     m_uploadMode == UploadMode::CLOUD ? "[MODE] Cloud" : "[MODE] Edge");
}

const char* ApiClient::getUploadModeString() const {
  switch (m_uploadMode) {
    case UploadMode::CLOUD: return "cloud";
    case UploadMode::EDGE: return "edge";
    case UploadMode::AUTO:
    default: return "auto";
  }
}

void ApiClient::buildLocalGatewayUrl(char* buffer, size_t bufferSize) {
  // Build URL: http://gateway-gh-{GH_ID}.local/api/data
  snprintf(buffer, bufferSize, "http://gateway-gh-%d.local/api/data", GH_ID);
}

UploadResult ApiClient::performLocalGatewayUpload(const char* payload, size_t length) {
  UploadResult result = {HTTPC_ERROR_CONNECTION_FAILED, false, "Gateway Connection Failed"};
  
  char gatewayUrl[64];
  buildLocalGatewayUrl(gatewayUrl, sizeof(gatewayUrl));
  
  LOG_INFO("GATEWAY", F("Uploading to local gateway: %s"), gatewayUrl);
  
  // Use plain WiFiClient for HTTP (not HTTPS)
  WiFiClient client;
  HTTPClient http;
  
  http.setTimeout(10000);  // 10s timeout for local network
  
  if (!http.begin(client, gatewayUrl)) {
    snprintf(result.message, sizeof(result.message), "HTTP Begin Failed");
    return result;
  }
  
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-Node-ID"), String(NODE_ID));
  http.addHeader(F("X-GH-ID"), String(GH_ID));
  
  result.httpCode = http.POST((uint8_t*)payload, length);
  result.success = (result.httpCode >= 200 && result.httpCode < 300);
  
  if (result.success) {
    snprintf(result.message, sizeof(result.message), "Gateway OK");
  } else if (result.httpCode < 0) {
    snprintf(result.message, sizeof(result.message), "%s", http.errorToString(result.httpCode).c_str());
  } else {
    snprintf(result.message, sizeof(result.message), "HTTP %d", result.httpCode);
  }
  
  http.end();
  return result;
}

void ApiClient::notifyLowMemory(uint32_t maxBlock, uint32_t totalFree) {
  LOG_WARN("MEM", F("Low Mem - Skip. Block: %u, Total: %u"), maxBlock, totalFree);
  char msg[80];
  snprintf(msg, sizeof(msg), "[SYSTEM] Upload Skipped: Low RAM (Free: %u, Blk: %u)", totalFree, maxBlock);
  broadcastEncrypted(msg);
}

unsigned long ApiClient::calculateBackoffInterval(const AppConfig& cfg) {
  unsigned long multiplier = 1;
  if (m_consecutiveUploadFailures < 16) {
    multiplier = (1UL << m_consecutiveUploadFailures);
  } else {
    multiplier = (1UL << 15);
  }

  unsigned long nextInterval = cfg.CACHE_SEND_INTERVAL_MS * multiplier;
  return (nextInterval > MAX_BACKOFF_MS) ? MAX_BACKOFF_MS : nextInterval;
}

void ApiClient::handleSuccessfulUpload(UploadResult& res, const AppConfig& cfg) {
  LOG_INFO("UPLOAD", F("Success: HTTP %d (%s)"), res.httpCode, res.message);
  m_lastApiSuccessMillis = millis();
  m_swWdtTimer.reset();

  if (m_consecutiveUploadFailures > 0) {
    m_consecutiveUploadFailures = 0;
    m_cacheSendTimer.setInterval(cfg.CACHE_SEND_INTERVAL_MS);
    LOG_INFO("UPLOAD", F("Backoff reset to normal interval."));
  }

  if (m_cacheManager.pop_one()) {
    snprintf(m_sharedBuffer, sizeof(m_sharedBuffer), "[SYSTEM] Upload OK (HTTP %d)", res.httpCode);
    broadcastEncrypted(m_sharedBuffer);
  } else {
    m_uploadState = UploadState::PAUSED;
  }
}

void ApiClient::handleFailedUpload(UploadResult& res, const AppConfig& cfg) {
  LOG_WARN("UPLOAD", F("Failed: %d (%s)"), res.httpCode, res.message);
  snprintf(m_sharedBuffer, sizeof(m_sharedBuffer), "[SYSTEM] Fail: %s (%d)", res.message, res.httpCode);
  broadcastEncrypted(m_sharedBuffer);

  m_uploadState = UploadState::PAUSED;
  m_cacheSendTimer.reset();

  m_consecutiveUploadFailures++;
  unsigned long nextInterval = calculateBackoffInterval(cfg);
  m_cacheSendTimer.setInterval(nextInterval);

  LOG_WARN("UPLOAD",
           F("Backoff active. Failures: %u. Next retry in: %lu s"),
           m_consecutiveUploadFailures,
           nextInterval / 1000);

  // Force WiFi reconnect after 5 consecutive failures
  if (m_consecutiveUploadFailures == 5) {
    LOG_WARN("API", F("5 consecutive failures. Toggling WiFi..."));
    WiFi.disconnect();
  }
}

bool ApiClient::executeQosSample(HTTPClient& http,
                                 const char* url,
                                 const char* method,
                                 const char* payload,
                                 const AppConfig& cfg,
                                 unsigned long& duration) {
  ESP.wdtFeed();
  yield();

  unsigned long startTick = millis();
  int httpCode = -1;

  if (http.begin(m_secureClient, url)) {
    char authBuf[128];
    snprintf(authBuf, sizeof(authBuf), "Bearer %s", cfg.AUTH_TOKEN.data());
    http.addHeader(F("Authorization"), authBuf);
    http.addHeader(F("User-Agent"), F("ESP8266-Node/QoS"));

    if (strcmp(method, "POST") == 0) {
      http.addHeader(F("Content-Type"), F("application/json"));
      httpCode = http.POST(payload);
    } else {
      httpCode = http.GET();
    }
    http.end();
  }

  duration = millis() - startTick;
  return (httpCode > 0);
}

void ApiClient::broadcastEncrypted(const char* text) {
  if (!text)
    return;

  // Use shared cipher and buffer to avoid duplicate BearSSL contexts and stack overflow
  char* buf = CryptoUtils::getEncryptionBuffer();
  size_t written = CryptoUtils::fast_serialize_encrypted(
      text, buf, CryptoUtils::ENCRYPTION_BUFFER_SIZE, CryptoUtils::getSharedCipher());
  if (written > 0) {
    m_ws.textAll(buf, written);
  }
}

ApiClient::ApiClient(AsyncWebSocket& ws,
                     NtpClient& ntpClient,
                     WifiManager& wifiManager,
                     ISensorManager& sensorManager,
                     BearSSL::WiFiClientSecure& secureClient,
                     ConfigManager& configManager,
                     ICacheManager& cacheManager)
    : m_ws(ws),
      m_ntpClient(ntpClient),
      m_wifiManager(wifiManager),
      m_sensorManager(sensorManager),
      m_secureClient(secureClient),
      m_configManager(configManager),
      m_cacheManager(cacheManager) {}

void ApiClient::init() {}

void ApiClient::applyConfig(const AppConfig& config) {
  m_dataCreationTimer.setInterval(config.DATA_UPLOAD_INTERVAL_MS);
  m_sampleTimer.setInterval(config.SENSOR_SAMPLE_INTERVAL_MS);
  m_cacheSendTimer.setInterval(config.CACHE_SEND_INTERVAL_MS);
  m_swWdtTimer.setInterval(config.SOFTWARE_WDT_TIMEOUT_MS);
  m_swWdtTimer.reset();
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
    createAndCachePayload();
  }
}

void ApiClient::checkSoftwareWdt() {
  if (m_lastApiSuccessMillis > 0 && 
      millis() - m_lastApiSuccessMillis > m_swWdtTimer.getInterval()) {
    LOG_ERROR("CRITICAL", F("Software WDT triggered. Rebooting!"));
    delay(1000);
    ESP.restart();
  }
}

// =============================================================================
// Main handle() - Refactored
// =============================================================================

void ApiClient::handle() {
  // Handle pending QoS task first
  if (m_pendingQosTask != QosTaskType::NONE) {
    handlePendingQosTask();
    m_pendingQosTask = QosTaskType::NONE;
    return;
  }

  // Require WiFi connection
  if (m_wifiManager.getState() != WifiManager::State::CONNECTED_STA) {
    m_uploadState = UploadState::IDLE;
    return;
  }

  // NTP fallback if not synced
  if (!m_ntpClient.isTimeSynced()) {
    tryNtpFallbackProbe();
  }

  handleTimerTasks();
  checkSoftwareWdt();
}

// --- NEW: QoS Implementation ---

void ApiClient::requestQosUpload() {
  m_pendingQosTask = QosTaskType::UPLOAD;
}

void ApiClient::requestQosOta() {
  m_pendingQosTask = QosTaskType::OTA;
}

void ApiClient::handlePendingQosTask() {
  const auto& cfg = m_configManager.getConfig();

  if (m_pendingQosTask == QosTaskType::UPLOAD) {
    const char* dummyJson = "{\"qos_test\":1}";
    performQosTest("Data Upload API", cfg.DATA_UPLOAD_URL.data(), "POST", dummyJson);
  } else if (m_pendingQosTask == QosTaskType::OTA) {
    // String Elimination: Reuse m_sharedBuffer for URL building
    snprintf(m_sharedBuffer, sizeof(m_sharedBuffer), "%s%d", cfg.FW_VERSION_CHECK_URL_BASE.data(), NODE_ID);
    performQosTest("OTA Version Check", m_sharedBuffer, "GET", "");
  }
}

// =============================================================================
// performQosTest Helper Methods
// =============================================================================

void ApiClient::updateQosStats(unsigned long duration, int& successCount, 
                               unsigned long& totalDuration, unsigned long& minLat, 
                               unsigned long& maxLat) {
  successCount++;
  totalDuration += duration;
  if (duration < minLat) minLat = duration;
  if (duration > maxLat) maxLat = duration;
}

void ApiClient::reportQosResults(const char* targetName, int successCount, int samples,
                                 unsigned long minLat, unsigned long maxLat, 
                                 unsigned long totalDuration) {
  float packetLoss = ((float)(samples - successCount) / samples) * 100.0f;
  float avgLat = (successCount > 0) ? ((float)totalDuration / successCount) : 0;
  float jitter = (successCount > 0) ? (float)(maxLat - minLat) : 0;

  snprintf(m_sharedBuffer, sizeof(m_sharedBuffer),
           "\n[REPORT] %s\n"
           " Requests    : %d/%d success\n"
           " Packet Loss : %.0f %%\n"
           " Latency (RT): Avg: %.0f ms | Min: %lu ms | Max: %lu ms\n"
           " Jitter      : %.0f ms\n"
           "-----------------------------",
           targetName, successCount, samples, packetLoss, avgLat, minLat, maxLat, jitter);
  
  broadcastEncrypted(m_sharedBuffer);
  LOG_INFO("QoS", F("Test Complete."));
}

// =============================================================================
// Main QoS Test - Refactored
// =============================================================================

void ApiClient::performQosTest(const char* targetName, const char* url, 
                               const char* method, const char* payload) {
  char msgBuf[128];
  snprintf(msgBuf, sizeof(msgBuf), "[QoS] Starting test for: %s...", targetName);
  broadcastEncrypted(msgBuf);
  LOG_INFO("QoS", F("Testing %s (%s)"), targetName, url);

  // Heap safety guard
  uint32_t freeBlock = ESP.getMaxFreeBlockSize();
  if (freeBlock < AppConstants::API_MIN_SAFE_BLOCK_SIZE) {
    LOG_ERROR("MEM", F("QoS Cancelled: Fragmentation too high! (Block: %u)"), freeBlock);
    broadcastEncrypted("[SYSTEM] QoS Cancelled: Low contiguous RAM. Try rebooting.");
    return;
  }

  int successCount = 0;
  unsigned long totalDuration = 0, minLat = 999999, maxLat = 0;
  constexpr int samples = 5;

  HTTPClient http;
  const auto& cfg = m_configManager.getConfig();
  http.setReuse(true);
  http.setTimeout(5000);

  for (int i = 0; i < samples; i++) {
    unsigned long duration = 0;
    if (executeQosSample(http, url, method, payload, cfg, duration)) {
      updateQosStats(duration, successCount, totalDuration, minLat, maxLat);
    } else {
      LOG_WARN("QoS", F("Req %d failed"), i + 1);
    }
    delay(100);
  }

  reportQosResults(targetName, successCount, samples, minLat, maxLat, totalDuration);
}

void ApiClient::scheduleImmediateUpload() {
  if (createAndCachePayload()) {
    if (m_uploadState == UploadState::IDLE) {
      m_uploadState = UploadState::UPLOADING;
    }
  } else {
    // --- FIX: Notify if cache write fails ---
    LOG_ERROR("API", F("Failed to write to cache (Full/Error)"));
    broadcastEncrypted("[SYSTEM] Error: Failed to save data to cache!");
  }
}

unsigned long ApiClient::getLastSuccessMillis() const {
  return m_lastApiSuccessMillis;
}

bool ApiClient::isHeapHealthy() {
  uint32_t maxBlock = ESP.getMaxFreeBlockSize();
  uint32_t totalFree = ESP.getFreeHeap();
  if (maxBlock < AppConstants::API_MIN_SAFE_BLOCK_SIZE || totalFree < AppConstants::API_MIN_TOTAL_HEAP) {
    LOG_WARN("MEM", F("Low Mem - Skip. Block: %u, Total: %u"), maxBlock, totalFree);
    return false;
  }
  return true;
}

bool ApiClient::createAndCachePayload() {
  const auto& cfg = m_configManager.getConfig();

  float tempVal = 0.0, humVal = 0.0;
  SensorReading t = m_sensorManager.getTemp();
  if (t.isValid)
    tempVal = round((t.value + cfg.TEMP_OFFSET) * 10.0) / 10.0;

  SensorReading h = m_sensorManager.getHumidity();
  if (h.isValid)
    humVal = round((h.value + cfg.HUMIDITY_OFFSET) * 10.0) / 10.0;

  uint16_t luxVal = 0;
  SensorReading l = m_sensorManager.getLight();
  if (l.isValid)
    luxVal = (uint16_t)(l.value * cfg.LUX_SCALING_FACTOR);

  long rssiVal = (m_sampleCount > 0) ? (m_rssiSum / m_sampleCount) : WiFi.RSSI();

  char timeStr[20];
  time_t now = time(nullptr);
  if (now > NTP_VALID_TIMESTAMP_THRESHOLD) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
  } else {
    snprintf(timeStr, sizeof(timeStr), "1970-01-01 00:00:00");
  }

  int written = snprintf(m_sharedBuffer,
                         sizeof(m_sharedBuffer),
                         "{\"gh_id\":%d,\"node_id\":%d,\"temperature\":%.1f,\"humidity\":%.1f,"
                         "\"light_intensity\":%u,\"rssi\":%ld,\"recorded_at\":\"%s\"}",
                         GH_ID,
                         NODE_ID,
                         tempVal,
                         humVal,
                         luxVal,
                         rssiVal,
                         timeStr);

  if (written < 0 || written >= (int)sizeof(m_sharedBuffer)) {
    LOG_ERROR("API", F("Payload truncated!"));
    return false;
  }

  m_rssiSum = 0;
  m_sampleCount = 0;

  return m_cacheManager.write(m_sharedBuffer, (size_t)written);
}

// Class method implementation (no longer a static function)
UploadResult ApiClient::performSingleUpload(const char* payload, size_t length) {
  UploadResult result = {HTTPC_ERROR_CONNECTION_FAILED, false, "Connection Failed"};

#ifdef DEBUG_API_UPLOAD
  LOG_DEBUG("API", F("--- START UPLOAD ---"));
  LOG_DEBUG("API", F("URL: %s"), m_configManager.getConfig().DATA_UPLOAD_URL.data());
  LOG_DEBUG("API", F("Length: %u"), length);
  LOG_DEBUG("API", F("Payload Content:\n%s"), payload);
  LOG_DEBUG("API", F("----------------------------"));
#endif

  HTTPClient http;
  const auto& cfg = m_configManager.getConfig();

  if (!http.begin(m_secureClient, cfg.DATA_UPLOAD_URL.data())) {
    snprintf(result.message, sizeof(result.message), "HTTP Begin Failed");
    return result;
  }

  // Collect headers for NTP fallback and redirects
  const char* defaultHeaders[] = {"Location"};
  const char* ntpHeaders[] = {"Date", "Location"};
  bool needsNtpSync = !m_ntpClient.isTimeSynced();
  http.collectHeaders(needsNtpSync ? ntpHeaders : defaultHeaders, needsNtpSync ? 2 : 1);

  setupHttpHeaders(http, cfg);
  result.httpCode = http.POST((uint8_t*)payload, length);
  result.success = (result.httpCode >= 200 && result.httpCode < 300);

  // Extract server time from Date header if NTP not synced
  if (result.httpCode > 0) {
    extractTimeFromResponse(http);
  }

  buildErrorMessage(result, http);
  http.end();

  return result;
}

// =============================================================================
// handleUploadCycle Helper Methods (Reduce Cyclomatic Complexity)
// =============================================================================

void ApiClient::processGatewayResult(const UploadResult& res) {
  if (res.success) {
    LOG_INFO("GATEWAY", F("Success: %s"), res.message);
    m_lastApiSuccessMillis = millis();
    m_swWdtTimer.reset();
    
    if (m_cacheManager.pop_one()) {
      snprintf(m_sharedBuffer, sizeof(m_sharedBuffer), "[GATEWAY] Upload OK");
      broadcastEncrypted(m_sharedBuffer);
    } else {
      m_uploadState = UploadState::PAUSED;
    }
  } else {
    LOG_WARN("GATEWAY", F("Failed: %s"), res.message);
    snprintf(m_sharedBuffer, sizeof(m_sharedBuffer), "[GATEWAY] Fail: %s", res.message);
    broadcastEncrypted(m_sharedBuffer);
    m_uploadState = UploadState::PAUSED;
  }
}

void ApiClient::handleEdgeModeUpload(size_t record_len) {
  LOG_INFO("UPLOAD", F("Sending %u bytes to gateway (EDGE mode)..."), record_len);
  UploadResult res = performLocalGatewayUpload(m_sharedBuffer, record_len);
  processGatewayResult(res);
}

void ApiClient::handleCloudModeUpload(size_t record_len, const AppConfig& cfg) {
  LOG_INFO("UPLOAD", F("Sending %u bytes to cloud (CLOUD mode)..."), record_len);
  UploadResult res = performSingleUpload(m_sharedBuffer, record_len);
  if (res.success) {
    handleSuccessfulUpload(res, cfg);
  } else {
    handleFailedUpload(res, cfg);
    // No gateway fallback in forced cloud mode
  }
}

bool ApiClient::tryCloudRecovery(size_t record_len, const AppConfig& cfg) {
  bool shouldTryCloud = (millis() - m_lastCloudRetryAttempt >= AppConstants::CLOUD_RETRY_INTERVAL_MS);
  if (!shouldTryCloud) return false;
  
  LOG_INFO("UPLOAD", F("Gateway mode: Retrying cloud..."));
  m_lastCloudRetryAttempt = millis();
  UploadResult res = performSingleUpload(m_sharedBuffer, record_len);
  
  if (res.success) {
    LOG_INFO("UPLOAD", F("Cloud recovered! Exiting gateway mode."));
    m_localGatewayMode = false;
    broadcastEncrypted("[SYSTEM] Cloud API recovered. Normal mode restored.");
    handleSuccessfulUpload(res, cfg);
    return true;
  }
  
  LOG_WARN("UPLOAD", F("Cloud still unreachable. Continuing gateway mode."));
  return false;
}

void ApiClient::handleAutoModeUpload(size_t record_len, const AppConfig& cfg) {
  if (m_localGatewayMode) {
    // Try cloud recovery first
    if (tryCloudRecovery(record_len, cfg)) {
      return;  // Cloud recovered, upload done
    }
    
    // Send to local gateway
    LOG_INFO("UPLOAD", F("Sending %u bytes to local gateway..."), record_len);
    UploadResult res = performLocalGatewayUpload(m_sharedBuffer, record_len);
    processGatewayResult(res);
  } else {
    // Normal cloud mode
    LOG_INFO("UPLOAD", F("Sending %u bytes to cloud..."), record_len);
    UploadResult res = performSingleUpload(m_sharedBuffer, record_len);
    
    if (res.success) {
      handleSuccessfulUpload(res, cfg);
    } else {
      handleFailedUpload(res, cfg);
      
      // Check if we should switch to gateway mode
      if (m_consecutiveUploadFailures >= AppConstants::LOCAL_GATEWAY_FALLBACK_THRESHOLD) {
        LOG_WARN("UPLOAD", F("Cloud unreachable after %u failures. Switching to gateway."),
                 m_consecutiveUploadFailures);
        m_localGatewayMode = true;
        m_lastCloudRetryAttempt = millis();
        broadcastEncrypted("[SYSTEM] Cloud unreachable. Switching to local gateway mode.");
      }
    }
  }
}

// =============================================================================
// Main Upload Cycle (Refactored)
// =============================================================================

void ApiClient::handleUploadCycle() {
  if (!m_ntpClient.isTimeSynced()) return;

  if (!isHeapHealthy()) {
    notifyLowMemory(ESP.getMaxFreeBlockSize(), ESP.getFreeHeap());
    m_uploadState = UploadState::PAUSED;
    return;
  }

  if (m_cacheManager.get_size() == 0) {
    m_uploadState = UploadState::IDLE;
    return;
  }

  ESP.wdtFeed();
  size_t record_len = 0;
  CacheReadError err = m_cacheManager.read_one(m_sharedBuffer, sizeof(m_sharedBuffer) - 1, record_len);

  if (err != CacheReadError::NONE) {
    if (err == CacheReadError::CORRUPT_DATA) {
      broadcastEncrypted("[SYSTEM] Cache corrupt record discarded.");
      return;
    }
    m_uploadState = UploadState::PAUSED;
    return;
  }

  m_sharedBuffer[record_len] = '\0';
  const auto& cfg = m_configManager.getConfig();
  
  // Dispatch to mode-specific handler
  switch (m_uploadMode) {
    case UploadMode::EDGE:
      handleEdgeModeUpload(record_len);
      break;
    case UploadMode::CLOUD:
      handleCloudModeUpload(record_len, cfg);
      break;
    case UploadMode::AUTO:
    default:
      handleAutoModeUpload(record_len, cfg);
      break;
  }

  if (m_uploadState == UploadState::PAUSED) {
    m_uploadState = UploadState::IDLE;
  }
}