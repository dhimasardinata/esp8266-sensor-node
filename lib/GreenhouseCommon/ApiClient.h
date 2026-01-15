#ifndef API_CLIENT_H
#define API_CLIENT_H
#include <Arduino.h>
#include <IntervalTimer.h>

#include "ConfigManager.h"

class AsyncWebSocket;
class NtpClient;
class WifiManager;
class ICacheManager;
class ISensorManager;
namespace BearSSL {
  class WiFiClientSecure;
}

// Upload mode for cloud/edge switching
enum class UploadMode : uint8_t {
  AUTO,   // Automatic fallback (default)
  CLOUD,  // Force cloud only
  EDGE    // Force local gateway only
};

struct UploadResult {
  int httpCode;
  bool success;
  char message[64];
};

class ApiClient {
public:
  using PayloadBuffer = char[MAX_PAYLOAD_SIZE + 1];
  ApiClient(AsyncWebSocket& ws,
            NtpClient& ntpClient,
            WifiManager& wifiManager,
            ISensorManager& sensorManager,
            BearSSL::WiFiClientSecure& secureClient,
            ConfigManager& configManager,
            ICacheManager& cacheManager);
  void init();
  void applyConfig(const AppConfig& config);
  void handle();
  void scheduleImmediateUpload();
  unsigned long getLastSuccessMillis() const;
  // --- NEW: QoS Methods ---
  void requestQosUpload();
  void requestQosOta();
  void broadcastEncrypted(const char* text);
  
  // --- Mode Control ---
  UploadMode getUploadMode() const { return m_uploadMode; }
  void setUploadMode(UploadMode mode);
  bool isLocalGatewayActive() const { return m_localGatewayMode; }
  const char* getUploadModeString() const;

private:
  PayloadBuffer m_sharedBuffer;
  bool isHeapHealthy();
  bool createAndCachePayload();
  void handleUploadCycle();
  // --- NEW: QoS Internal Logic ---
  enum class QosTaskType { NONE, UPLOAD, OTA };
  QosTaskType m_pendingQosTask = QosTaskType::NONE;
  void handlePendingQosTask();
  void performQosTest(const char* targetName, const char* url, const char* method, const char* payload);
  UploadResult performSingleUpload(const char* payload, size_t length);
  
  // --- Local Gateway Fallback ---
  UploadResult performLocalGatewayUpload(const char* payload, size_t length);
  void buildLocalGatewayUrl(char* buffer, size_t bufferSize);
  
  // --- handleUploadCycle helpers (reduce cyclomatic complexity) ---
  void handleEdgeModeUpload(size_t record_len);
  void handleCloudModeUpload(size_t record_len, const AppConfig& cfg);
  void handleAutoModeUpload(size_t record_len, const AppConfig& cfg);
  bool tryCloudRecovery(size_t record_len, const AppConfig& cfg);
  void processGatewayResult(const UploadResult& res);
  
  // --- handle() helpers ---
  void handleTimerTasks();
  void checkSoftwareWdt();
  
  // --- performQosTest helpers ---
  void updateQosStats(unsigned long duration, int& successCount, unsigned long& totalDuration,
                      unsigned long& minLat, unsigned long& maxLat);
  void reportQosResults(const char* targetName, int successCount, int samples,
                        unsigned long minLat, unsigned long maxLat, unsigned long totalDuration);
  
  // --- Helper functions to reduce cyclomatic complexity ---
  // performSingleUpload helpers
  void setupHttpHeaders(class HTTPClient& http, const AppConfig& cfg);
  void extractTimeFromResponse(class HTTPClient& http);
  void buildErrorMessage(UploadResult& result, class HTTPClient& http);
  
  // handleUploadCycle helpers
  void notifyLowMemory(uint32_t maxBlock, uint32_t totalFree);
  void handleSuccessfulUpload(UploadResult& res, const AppConfig& cfg);
  void handleFailedUpload(UploadResult& res, const AppConfig& cfg);
  unsigned long calculateBackoffInterval(const AppConfig& cfg);
  
  // handle() helpers
  void tryNtpFallbackProbe();
  
  // performQosTest helpers
  bool executeQosSample(class HTTPClient& http, const char* url, const char* method, 
                        const char* payload, const AppConfig& cfg, unsigned long& duration);
  AsyncWebSocket& m_ws;
  NtpClient& m_ntpClient;
  WifiManager& m_wifiManager;
  ISensorManager& m_sensorManager;
  BearSSL::WiFiClientSecure& m_secureClient;
  ConfigManager& m_configManager;
  ICacheManager& m_cacheManager;
  IntervalTimer m_dataCreationTimer;
  IntervalTimer m_sampleTimer;
  IntervalTimer m_cacheSendTimer;
  IntervalTimer m_swWdtTimer;
  long m_rssiSum = 0;
  uint8_t m_sampleCount = 0;
  unsigned long m_lastApiSuccessMillis = 0;
  enum class UploadState { IDLE, UPLOADING, PAUSED };
  UploadState m_uploadState = UploadState::IDLE;
  uint32_t m_consecutiveUploadFailures = 0;
  unsigned long m_lastTimeProbe = 0;  // HTTP time probe tracking
  static constexpr unsigned long MAX_BACKOFF_MS = 3600000;
  
  // --- Mode & Gateway State ---
  UploadMode m_uploadMode = UploadMode::AUTO;
  bool m_localGatewayMode = false;
  unsigned long m_lastCloudRetryAttempt = 0;
};
#endif