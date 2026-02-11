#ifndef API_CLIENT_H
#define API_CLIENT_H
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <IntervalTimer.h>
#include <bearssl/bearssl_hmac.h>
#include <array>
#include <memory>
#include <string_view>

#include "ConfigManager.h"

class AsyncWebSocket;
class NtpClient;
class WifiManager;
class CacheManager;   // Concrete type for CRTP
class SensorManager;  // Concrete type for CRTP
namespace BearSSL {
  class WiFiClientSecure;
  class X509List;
}

// Upload mode for cloud/edge switching
enum class UploadMode : uint8_t {
  AUTO,   // Automatic fallback (default)
  CLOUD,  // Force cloud only
  EDGE    // Force local gateway only
};

struct UploadResult {
  int16_t httpCode;
  uint8_t success;
  char message[32];
};

class ApiClient final {
public:
  using PayloadBuffer = std::array<char, MAX_PAYLOAD_SIZE + 1>;
  static_assert(MAX_PAYLOAD_SIZE >= 256, "Payload buffer too small for JSON");
  ApiClient(AsyncWebSocket& ws,
            NtpClient& ntpClient,
            WifiManager& wifiManager,
            SensorManager& sensorManager,
            BearSSL::WiFiClientSecure& secureClient,
            ConfigManager& configManager,
            CacheManager& cacheManager,
            const BearSSL::X509List* trustAnchors);
  ~ApiClient();

  // Disable copying to satisfy -Werror=effc++ for m_activeClient pointer member
  ApiClient(const ApiClient&) = delete;
  ApiClient& operator=(const ApiClient&) = delete;

  void init();
  void applyConfig(const AppConfig& config);
  void handle();
  void scheduleImmediateUpload();
  void requestImmediateUpload();  // Request blocking upload from main loop
  unsigned long getLastSuccessMillis() const;
  // --- NEW: QoS Methods ---
  void requestQosUpload();
  void requestQosOta();
  void broadcastEncrypted(std::string_view text);
  void broadcastEncrypted(const char* text);

  // --- Mode Control ---
  UploadMode getUploadMode() const {
    return m_uploadMode;
  }
  void setUploadMode(UploadMode mode);
  bool isLocalGatewayActive() const {
    return m_localGatewayMode;
  }
  const char* getUploadModeString() const;

  void pause();
  void resume();
  void setTrustAnchors(const BearSSL::X509List* trustAnchors);
  void setOtaInProgress(bool inProgress);
  [[nodiscard]] bool isUploadActive() const;

private:
  std::unique_ptr<PayloadBuffer> m_sharedBuffer;
  std::unique_ptr<BearSSL::X509List> m_localTrustAnchors;
  bool m_tlsActive = false;
  bool m_tlsInsecure = false;
  bool m_otaInProgress = REDACTED
  [[nodiscard]] bool ensureSharedBuffer();
  void releaseSharedBuffer();
  [[nodiscard]] char* sharedBuffer();
  [[nodiscard]] const char* sharedBuffer() const;
  [[nodiscard]] size_t sharedBufferSize() const;
  [[nodiscard]] bool ensureTrustAnchors();
  [[nodiscard]] const BearSSL::X509List* activeTrustAnchors() const;
  [[nodiscard]] bool acquireTlsResources(bool allowInsecure);
  void releaseTlsResources();
  [[nodiscard]] bool isHeapHealthy();
  [[nodiscard]] bool createAndCachePayload();
  void handleUploadCycle();

  // --- Non-Blocking Upload State Machine ---
  enum class HttpState { IDLE, CONNECTING, SENDING_REQUEST, WAITING_RESPONSE, READING_RESPONSE, COMPLETE, FAILED };
  HttpState m_httpState = HttpState::IDLE;
  unsigned long m_stateEntryTime = 0;
  size_t m_payloadLen = 0;
  UploadResult m_lastResult;
  WiFiClient* m_activeClient = REDACTED

  void updateResult(int code, bool success, const char* msg);
  size_t prepareEdgePayload(size_t rawLen);
  void startUpload(const char* payload, size_t length, bool isEdgeTarget);
  void handleUploadStateMachine();
  void transitionState(HttpState newState);

  // State Handlers
  void handleStateConnecting(const AppConfig& cfg);
  void handleStateSending(const AppConfig& cfg);
  void handleStateWaiting(unsigned long stateDuration);
  void handleStateReading();

  // --- NEW: QoS Internal Logic ---
  enum class QosTaskType { NONE, UPLOAD, OTA };
  QosTaskType m_pendingQosTask = QosTaskType::NONE;
  bool m_qosActive = false;
  uint8_t m_qosSampleIdx = 0;
  unsigned long m_qosNextAt = 0;
  int m_qosSuccessCount = 0;
  unsigned long m_qosTotalDuration = REDACTED
  unsigned long m_qosMinLat = 0;
  unsigned long m_qosMaxLat = 0;
  const char* m_qosTargetName = nullptr;
  struct QosBuffers {
    char url[160] = {0};
    char method[8] = {0};
    char payload[64] = {0};
  };
  std::unique_ptr<QosBuffers> m_qosBuffers;
  void handlePendingQosTask();
  void performQosTest(const char* targetName, const char* url, const char* method, const char* payload);
  [[nodiscard]] UploadResult performSingleUpload(const char* payload, size_t length, bool allowInsecure);
  UploadResult performImmediateUpload();  // Blocking upload - called from main loop only
  void signPayload(const char* payload, size_t payload_len, char* signatureBuffer);

  // --- Local Gateway Fallback ---
  [[nodiscard]] UploadResult performLocalGatewayUpload(const char* payload, size_t length);
  void buildLocalGatewayUrl(char* buffer, size_t bufferSize);

  // --- NEW: Centralized Mode Control ---
  int checkGatewayMode();  // Returns 0(Cloud), 1(Local), 2(Auto), or -1(Fail)

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
  void updateQosStats(unsigned long duration,
                      int& successCount,
                      unsigned long& totalDuration,
                      unsigned long& minLat,
                      unsigned long& maxLat);
  void reportQosResults(const char* targetName,
                        int successCount,
                        int samples,
                        unsigned long minLat,
                        unsigned long maxLat,
                        unsigned long totalDuration);

  // --- Helper functions to reduce cyclomatic complexity ---
  // performSingleUpload helpers
  void setupHttpHeaders(class HTTPClient& http, const AppConfig& cfg);
  void extractTimeFromResponse(class HTTPClient& http);
  void buildErrorMessage(UploadResult& result, class HTTPClient& http);
  void updateCloudTargetCache();

  // handleUploadCycle helpers
  void notifyLowMemory(uint32_t maxBlock, uint32_t totalFree);
  void handleSuccessfulUpload(UploadResult& res, const AppConfig& cfg);
  void handleFailedUpload(UploadResult& res, const AppConfig& cfg);
  void trackUploadFailure();  // Centralized failure logic
  unsigned long calculateBackoffInterval(const AppConfig& cfg);
  void prepareTlsHeap();
  void broadcastUploadTarget(bool isEdge);
  static constexpr int kImmediateDeferred = -2000;

  // handle() helpers
  void tryNtpFallbackProbe();

  // performQosTest helpers
  bool executeQosSample(class HTTPClient& http,
                        const char* url,
                        const char* method,
                        const char* payload,
                        const AppConfig& cfg,
                        unsigned long& duration);
  AsyncWebSocket& m_ws;
  NtpClient& m_ntpClient;
  WifiManager& m_wifiManager;
  SensorManager& m_sensorManager;
  BearSSL::WiFiClientSecure& m_secureClient;
  ConfigManager& m_configManager;
  CacheManager& m_cacheManager;
  const BearSSL::X509List* m_trustAnchors = nullptr;
  char m_cloudHost[64] = {0};
  char m_cloudPath[48] = {0};
  IntervalTimer m_dataCreationTimer;
  IntervalTimer m_sampleTimer;
  IntervalTimer m_cacheSendTimer;
  IntervalTimer m_cacheFlushTimer;
  IntervalTimer m_swWdtTimer;
  int32_t m_rssiSum = 0;
  uint16_t m_sampleCount = 0;
  char m_cachedTimeStr[20] = "1970-01-01 00:00:00";
  time_t m_cachedTimeEpoch = 0;
  unsigned long m_lastApiSuccessMillis = 0;
  enum class UploadState { IDLE, UPLOADING, PAUSED };
  UploadState m_uploadState = UploadState::IDLE;
  uint32_t m_consecutiveUploadFailures = 0;
  unsigned long m_lastTimeProbe = 0;                       // HTTP time probe tracking
  static constexpr unsigned long MAX_BACKOFF_MS = 300000;  // 5 minutes cap (was 1 hour)
  static constexpr unsigned long GATEWAY_MODE_TTL_MS = 30000;

  // --- Mode & Gateway State ---
  UploadMode m_uploadMode = UploadMode::AUTO;
  bool m_localGatewayMode = false;
  bool m_targetIsEdge = false;  // Track destination for current upload attempt
  unsigned long m_lastCloudRetryAttempt = 0;
  int8_t m_cachedGatewayMode = -1;
  unsigned long m_lastGatewayModeCheck = 0;
  bool m_currentRecordSentToGateway = false;  // Track if gateway was notified for current record
  bool m_immediateUploadRequested = false;    // Flag for deferred immediate upload
  uint8_t m_immediateWarmup = 0;
  unsigned long m_lastImmediateDeferLog = 0;
  unsigned long m_immediateRetryAt = 0;
  int8_t m_immediateGatewayMode = -2;
  bool m_immediatePollReady = false;
  bool m_isSystemPaused = false;
  uint8_t m_lowMemCounter = 0;

  // Reuse to avoid stack allocation
  HTTPClient m_httpClient;
  WiFiClient m_plainClient;
};
#endif
