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
#include "NtpClient.h"
#include "WifiManager.h"
#include "node_config.h"
#include "root_ca_data.h"
#include "sensor_data.h"
#include "utils.h"
#include "constants.h"
#include "Logger.h"


static void broadcastEncrypted(AsyncWebSocket& ws, const char* text) {
  CryptoUtils::AES_CBC_Cipher cipher(std::string_view(reinterpret_cast<const char*>(CryptoUtils::AES_KEY), 32));
  auto encrypted = cipher.encrypt(text);
  if (encrypted) {
    ws.textAll(CryptoUtils::serialize_payload(*encrypted));
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

void ApiClient::handle() {
  // --- NEW: Handle Pending QoS Task in Main Loop ---
  if (m_pendingQosTask != QosTaskType::NONE) {
    handlePendingQosTask();
    m_pendingQosTask = QosTaskType::NONE;  // Clear task
    return;                                // Skip other tasks this loop to save resources
  }

  if (m_wifiManager.getState() != WifiManager::State::CONNECTED_STA) {
    m_uploadState = UploadState::IDLE;
    return;
  }

  // --- NTP FALLBACK LOGIC ---
  if (!m_ntpClient.isTimeSynced()) {
    static unsigned long lastTimeProbe = 0;
    // If NTP is stuck for > 60 seconds (Wait for initial NTP try), try HTTP Probe
    if (millis() > 60000 && millis() - lastTimeProbe > 60000) {
      lastTimeProbe = millis();
      LOG_WARN("TIME", F("NTP stuck. Probing HTTP server for 'Date' header..."));
      // Send a dummy request just to get headers
      // We use performSingleUpload with empty payload to minimize data usage
      // The server might reject (400 Bad Request), but it WILL send a Date header!
      performSingleUpload("{}", 2);
    }
  }

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

  if (m_lastApiSuccessMillis > 0 && millis() - m_lastApiSuccessMillis > m_swWdtTimer.getInterval()) {
    LOG_ERROR("CRITICAL", F("Software WDT triggered. Rebooting!"));
    delay(1000);
    ESP.restart();
  }
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
    String fullUrl = String(cfg.FW_VERSION_CHECK_URL_BASE.data()) + String(NODE_ID);
    performQosTest("OTA Version Check", fullUrl.c_str(), "GET", "");
  }
}

void ApiClient::performQosTest(const char* targetName, const char* url, const char* method, const char* payload) {
  // Notify user that test is starting
  char msgBuf[128];
  snprintf(msgBuf, sizeof(msgBuf), "[QoS] Starting test for: %s...", targetName);
  broadcastEncrypted(m_ws, msgBuf);
  LOG_INFO("QoS", F("Testing %s (%s)"), targetName, url);

  const int samples = 5;
  int successCount = 0;
  unsigned long totalDuration = 0;
  unsigned long minLat = 999999;
  unsigned long maxLat = 0;

  HTTPClient http;
  const auto& cfg = m_configManager.getConfig();


  http.setReuse(true);  // Keep-Alive
  http.setTimeout(5000);

  for (int i = 0; i < samples; i++) {
    // --- CRITICAL: Feed WDT and Yield to prevent crashes ---
    ESP.wdtFeed();
    yield();

    unsigned long startTick = millis();
    bool requestSuccess = false;
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

      if (httpCode > 0) {
        requestSuccess = true;
      }
      http.end();
    }

    unsigned long duration = millis() - startTick;

    if (requestSuccess) {
      successCount++;
      totalDuration += duration;
      if (duration < minLat)
        minLat = duration;
      if (duration > maxLat)
        maxLat = duration;
    } else {
      LOG_WARN("QoS", F("Req %d failed: %d"), i + 1, httpCode);
    }

    // Small delay between requests
    delay(100);
  }

  // --- Calculate Metrics ---
  float packetLoss = ((float)(samples - successCount) / samples) * 100.0f;
  float avgLat = (successCount > 0) ? ((float)totalDuration / successCount) : 0;
  float jitter = (successCount > 0) ? (float)(maxLat - minLat) : 0;

  // --- Report Result ---
  char reportBuf[512];
  int len = snprintf(reportBuf,
                     sizeof(reportBuf),
                     "\n[REPORT] %s\n"
                     " Requests    : %d/%d success\n"
                     " Packet Loss : %.0f %%\n"
                     " Latency (RT): Avg: %.0f ms | Min: %lu ms | Max: %lu ms\n"
                     " Jitter      : %.0f ms\n"
                     "-----------------------------",
                     targetName,
                     successCount,
                     samples,
                     packetLoss,
                     avgLat,
                     minLat,
                     maxLat,
                     jitter);

  if (len > 0) {
    broadcastEncrypted(m_ws, reportBuf);
  }
  LOG_INFO("QoS", F("Test Complete."));
}

void ApiClient::scheduleImmediateUpload() {
  if (createAndCachePayload()) {
    if (m_uploadState == UploadState::IDLE) {
      m_uploadState = UploadState::UPLOADING;
    }
  } else {
    // --- FIX: Notify if cache write fails ---
    LOG_ERROR("API", F("Failed to write to cache (Full/Error)"));
    broadcastEncrypted(m_ws, "[SYSTEM] Error: Failed to save data to cache!");
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
  // Init default: Error -1 (Connection Failed)
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

  if (http.begin(m_secureClient, cfg.DATA_UPLOAD_URL.data())) {
    // --- NEW: Timeouts & Heap Optimization ---
    http.setTimeout(15000);  // Prevent WDT resets on slow servers

    if (!m_ntpClient.isTimeSynced()) {
      const char* headerKeys[] = {"Date"};
      http.collectHeaders(headerKeys, 1);
    }

    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.addHeader(F("Connection"), F("close"));
    http.addHeader(F("Accept"), F("application/json"));
    http.addHeader(F("Content-Type"), F("application/json"));
    http.addHeader(F("User-Agent"),
                   F("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                     "Chrome/120.0.0.0 Safari/537.36"));

    char authBuffer[MAX_TOKEN_LEN + 20];
    // Optimize: Use raw buffer directly, avoiding String() heap allocation.
    // Assumes AUTH_TOKEN is cleanly null-terminated and trimmed by ConfigManager.
    snprintf(authBuffer, sizeof(authBuffer), "Bearer %s", cfg.AUTH_TOKEN.data());
    // NOTE: Auth header intentionally not logged for security
    http.addHeader(F("Authorization"), authBuffer);
    result.httpCode = http.POST((uint8_t*)payload, length);
    if (result.httpCode == 301 || result.httpCode == 302 || result.httpCode == 307 || result.httpCode == 308) {
      String newLocation = http.header("Location");  // Get Location header
      if (newLocation.length() == 0) {
        // Sometimes the library needs to be told to collect certain headers
        const char* headerKeys[] = {"Location"};
        http.collectHeaders(headerKeys, 1);
        // Retry POST or check collectHeaders logic (usually must be before POST)
        // Easiest solution: Test URL via Postman/Browser/Curl on PC
      }
      LOG_DEBUG("API", F("Server asks redirect to: %s"), http.getLocation().c_str());
    }
    result.success = (result.httpCode >= 200 && result.httpCode < 300);
    // --- NEW: Process Date Header ---
    // Use the header if the request got a response (even an error 400/500 usually has a Date)
    if (result.httpCode > 0 && !m_ntpClient.isTimeSynced()) {
      String dateHeader = http.header("Date");
      if (dateHeader.length() > 0) {
        time_t serverTime = Utils::parse_http_date(dateHeader.c_str());
        if (serverTime > 0) {
          m_ntpClient.setManualTime(serverTime);
        }
      }
    }
    // ERROR MESSAGE LOGIC (Without String Object)
    if (result.success) {
      snprintf(result.message, sizeof(result.message), "OK");
    } else {
      // Case 1: Internal ESP Error (Negative Number)
      if (result.httpCode < 0) {
        snprintf(result.message, sizeof(result.message), "%s", http.errorToString(result.httpCode).c_str());
      }
      // Case 2: HTTP Server Error (Positive Number)
      else {
        // Data-driven HTTP error lookup table
        static constexpr struct { int code; const char* reason; } httpErrors[] = {
            {400, "Bad Request"}, {401, "Unauthorized"}, {403, "Forbidden"},
            {404, "Not Found"}, {419, "Session Expired"}, {422, "Unprocessable"},
            {429, "Too Many Requests"}, {500, "Server Error"}
        };
        
        const char* reason = "Error";  // Default
        for (const auto& err : httpErrors) {
          if (result.httpCode == err.code) {
            reason = err.reason;
            break;
          }
        }
        
        // Format output: "HTTP 401 (Unauthorized)"
        snprintf(result.message, sizeof(result.message), "HTTP %d (%s)", result.httpCode, reason);
      }
    }

    http.end();
  } else {
    // Error message if http.begin fails
    snprintf(result.message, sizeof(result.message), "HTTP Begin Failed");
  }
  return result;
}

void ApiClient::handleUploadCycle() {
  if (!m_ntpClient.isTimeSynced())
    return;
  if (!isHeapHealthy()) {
    char msg[80];
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    uint32_t totalFree = ESP.getFreeHeap();

    // Print to Serial for debug
    LOG_WARN("MEM", F("Low Mem - Skip. Block: %u, Total: %u"), maxBlock, totalFree);

    // Send notification via WebSocket
    snprintf(msg, sizeof(msg), "[SYSTEM] Upload Skipped: Low RAM (Free: %u, Blk: %u)", totalFree, maxBlock);
    broadcastEncrypted(m_ws, msg);

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
      // Notify if corrupt data was discarded
      broadcastEncrypted(m_ws, "[SYSTEM] Cache corrupt record discarded.");
      return;
    }
    m_uploadState = UploadState::PAUSED;
    return;
  }

  // FORCE Null-Terminate (Safety against overread)
  m_sharedBuffer[record_len] = '\0';

  LOG_INFO("UPLOAD", F("Sending %u bytes..."), record_len);

  UploadResult res = performSingleUpload(m_sharedBuffer, record_len);

  char notifBuffer[96];
  const auto& cfg = m_configManager.getConfig();  // Need config to get default interval

  if (res.success) {
    LOG_INFO("UPLOAD", F("Success: HTTP %d (%s)"), res.httpCode, res.message);
    m_lastApiSuccessMillis = millis();
    m_swWdtTimer.reset();

    // --- BACKOFF RESET ---
    // Connection is good, reset failure count and restore default interval
    if (m_consecutiveUploadFailures > 0) {
      m_consecutiveUploadFailures = 0;
      m_cacheSendTimer.setInterval(cfg.CACHE_SEND_INTERVAL_MS);
      LOG_INFO("UPLOAD", F("Backoff reset to normal interval."));
    }
    // ---------------------

    if (m_cacheManager.pop_one()) {
      snprintf(notifBuffer, sizeof(notifBuffer), "[SYSTEM] Upload OK (HTTP %d)", res.httpCode);
      broadcastEncrypted(m_ws, notifBuffer);
    } else {
      m_uploadState = UploadState::PAUSED;
    }
  } else {
    LOG_WARN("UPLOAD", F("Failed: %d (%s)"), res.httpCode, res.message);

    snprintf(notifBuffer, sizeof(notifBuffer), "[SYSTEM] Fail: %s (%d)", res.message, res.httpCode);
    broadcastEncrypted(m_ws, notifBuffer);

    m_uploadState = UploadState::PAUSED;
    m_cacheSendTimer.reset();

    // --- EXPONENTIAL BACKOFF LOGIC ---
    m_consecutiveUploadFailures++;

    // Calculate new interval: Default * 2^failures
    // We use bitshift (1 << failures) for power of 2
    unsigned long multiplier = 1;
    if (m_consecutiveUploadFailures < 16) {  // Prevent overflow
      multiplier = (1UL << m_consecutiveUploadFailures);
    } else {
      multiplier = (1UL << 15);
    }

    unsigned long nextInterval = cfg.CACHE_SEND_INTERVAL_MS * multiplier;

    // Cap at MAX_BACKOFF_MS
    if (nextInterval > MAX_BACKOFF_MS) {
      nextInterval = MAX_BACKOFF_MS;
    }

    m_cacheSendTimer.setInterval(nextInterval);

    LOG_WARN("UPLOAD", F("Backoff active. Failures: %u. Next retry in: %lu s"),
                    m_consecutiveUploadFailures,
                    nextInterval / 1000);
    // ---------------------------------

    // NEW: If we have failed 5 times in a row, force a WiFi reconnect
    // This fixes "Zombie" associations without needing a full system reboot
    if (m_consecutiveUploadFailures == 5) {
      LOG_WARN("API", F("5 consecutive failures. Toggling WiFi..."));
      WiFi.disconnect();
      // WifiManager will catch the disconnect event and reconnect automatically
    }
  }

  if (m_uploadState == UploadState::PAUSED)
    m_uploadState = UploadState::IDLE;
}