#ifndef API_CLIENT_H
#define API_CLIENT_H
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <system/IntervalTimer.h>
#include <bearssl/bearssl_hmac.h>
#include <memory>
#include <string_view>

#include "api/ApiClient.Context.h"
class ApiClientLifecycleController;
class ApiClientControlController;
class ApiClientQueueController;
class ApiClientTransportController;
class ApiClientUploadController;
class ApiClientUploadRuntimeController;
class ApiClientQosController;

// Upload mode for cloud/edge switching
enum class UploadMode : uint8_t {
  AUTO,   // Automatic fallback (default)
  CLOUD,  // Force cloud only
  EDGE    // Force local gateway only
};

class ApiClient final {
public:
  using PayloadBuffer = ApiClientDetail::PayloadBuffer;
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

  // Facade owns runtime state buckets and should not be copied.
  ApiClient(const ApiClient&) = delete;
  ApiClient& operator=(const ApiClient&) = delete;

  void init();
  void applyConfig(const AppConfig& config);
  void handle();
  void scheduleImmediateUpload();
  void requestImmediateUpload(bool restoreWsAfterUpload);
  void requestImmediateUpload();  // Request blocking upload from main loop
  unsigned long getLastSuccessMillis() const;
  [[nodiscard]] uint8_t getEmergencyQueueDepth() const {
    return m_runtime.queue.emergencyCount;
  }
  [[nodiscard]] uint8_t getEmergencyQueueCapacity() const {
    return kEmergencyQueueCapacity;
  }
  [[nodiscard]] bool isEmergencyBackpressureActive() const {
    return m_runtime.queue.emergencyBackpressure;
  }
  // --- NEW: QoS Methods ---
  void requestQosUpload();
  void requestQosOta();
  void broadcastEncrypted(std::string_view text);
  void broadcastEncrypted(const char* text);
  void broadcastEncrypted(const __FlashStringHelper* text);

  // --- Mode Control ---
  UploadMode getUploadMode() const {
    return m_runtime.route.uploadMode;
  }
  void setUploadMode(UploadMode mode);
  UplinkMode getUplinkMode() const {
    return m_runtime.route.uplinkMode;
  }
  bool isLocalGatewayActive() const {
    return m_runtime.route.localGatewayMode;
  }
  void copyUploadModeString(char* out, size_t out_len) const;
  void copyUplinkModeString(char* out, size_t out_len) const;
  void copyActiveCloudRouteString(char* out, size_t out_len) const;

  void pause();
  void resume();
  void setTrustAnchors(const BearSSL::X509List* trustAnchors);
  void setOtaInProgress(bool inProgress);
  [[nodiscard]] bool isUploadActive() const;

private:
  friend class ApiClientLifecycleController;
  friend class ApiClientControlController;
  friend class ApiClientQueueController;
  friend class ApiClientTransportController;
  friend class ApiClientUploadController;
  friend class ApiClientUploadRuntimeController;
  friend class ApiClientQosController;
  using UploadRecordSource = ApiClientDetail::UploadRecordSource;
  using UploadRecordLoad = ApiClientDetail::UploadRecordLoad;
  using EmergencyRecord = ApiClientDetail::EmergencyRecord;
  using EmergencyQueueReason = ApiClientDetail::EmergencyQueueReason;
  static constexpr uint8_t kEmergencyQueueCapacity = ApiClientDetail::kEmergencyQueueCapacity;
  using RoutingState = ApiClientDetail::RoutingState;
  using QueueState = ApiClientDetail::QueueState;
  using ImmediateUploadState = ApiClientDetail::ImmediateUploadState;
  using QueuedUploadTargetDecision = ApiClientDetail::QueuedUploadTargetDecision;
  using UploadState = ApiClientDetail::UploadState;
  using HttpState = ApiClientDetail::HttpState;
  using QosTaskType = ApiClientDetail::QosTaskType;
  using QosBuffers = ApiClientDetail::QosBuffers;
  using DependencyRefs = ApiClientDetail::DependencyRefs;
  using OperationalState = ApiClientDetail::OperationalState;
  using TransportRuntime = ApiClientDetail::TransportRuntime;
  using QosRuntime = ApiClientDetail::QosRuntime;
  using ResourceState = ApiClientDetail::ResourceState;
  using GuardPolicy = ApiClientDetail::GuardPolicy;
  using RuntimeHealth = ApiClientDetail::RuntimeHealth;
  using ControllerContext = ApiClientDetail::ControllerContext;
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
  void flushRtcToLittleFs();
  static PGM_P uploadSourceLabelP(UploadRecordSource source);
  static void copyUploadSourceLabel(char* out, size_t out_len, UploadRecordSource source);
  UploadRecordLoad loadRecordForUpload(size_t& record_len);
  UploadRecordLoad loadRecordFromRtc(size_t& record_len);
  UploadRecordLoad loadRecordFromLittleFs(size_t& record_len);
  [[nodiscard]] bool enqueueEmergencyRecord(const EmergencyRecord& record);
  [[nodiscard]] bool peekEmergencyRecord(EmergencyRecord& out) const;
  [[nodiscard]] bool popEmergencyRecord(EmergencyRecord& out);
  void drainEmergencyQueueToStorage(uint8_t maxRecords);
  [[nodiscard]] bool persistEmergencyRecord(const EmergencyRecord& record, bool allowDirectSend);
  void populateEmergencyRecord(EmergencyRecord& outRecord);
  [[nodiscard]] bool buildPayloadFromEmergencyRecord(const EmergencyRecord& record,
                                                     char* out,
                                                     size_t out_len,
                                                     size_t& payload_len) const;
  [[nodiscard]] bool appendEmergencyRecordToRtc(const EmergencyRecord& record, bool announce);
  [[nodiscard]] bool appendEmergencyRecordToLittleFs(const EmergencyRecord& record, bool announce);
  [[nodiscard]] bool tryDirectSendEmergencyRecord(const EmergencyRecord& record, UploadResult& result);
  void resetSampleAccumulator();
  void clearCurrentRecordFlags();
  void clearLoadedRecordContext();
  void resetQueuePopRecovery();
  void resetRtcFallbackRecovery();
  void copyCurrentUploadSourceLabel(char* out, size_t out_len) const;
  void broadcastUploadDispatch(bool immediate, bool isTargetEdge, size_t payload_len);
  bool finishLoadedRecordSuccess(const AppConfig& cfg, int httpCode, bool setIdleOnPopFailure);
  void resetQueuedUploadCycle(bool resetTimer);
  bool dispatchQueuedUploadRecord(size_t record_len, bool isTargetEdge);
  [[nodiscard]] bool recoverPendingQueuePop(const AppConfig& cfg, bool immediate, UploadResult* result);
  void logEmergencyQueueState(EmergencyQueueReason reason);
  [[nodiscard]] bool trySendLiveSnapshotToGateway();
  bool popLoadedRecord();
  void applyQueuePopFailureCooldown(const AppConfig& cfg, const char* sourceTag);
  [[nodiscard]] QueuedUploadTargetDecision resolveQueuedUploadTarget(bool& isTargetEdge);
  void handleUploadCycle();
  [[nodiscard]] static const char* gatewayModeLabel(int mode);

  void updateResult(int code, bool success, const char* msg);
  void updateResult_P(int code, bool success, PGM_P msg);
  size_t prepareEdgePayload(size_t rawLen);
  void startUpload(const char* payload, size_t length, bool isEdgeTarget);
  void handleUploadStateMachine();
  void transitionState(HttpState newState);

  // State Handlers
  void handleStateConnecting(const AppConfig& cfg);
  void handleStateSending(const AppConfig& cfg);
  void handleStateWaiting(unsigned long stateDuration);
  void handleStateReading();
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
  void processGatewayResult(const UploadResult& res);
  void markImmediateUploadDeferred(UploadResult& result);
  void resetImmediateUploadPollState();
  [[nodiscard]] bool prepareImmediateUploadRecord(UploadResult& result, size_t& record_len);
  [[nodiscard]] bool resolveImmediateUploadTarget(UploadResult& result, bool& isTargetEdge);
  void finalizeImmediateUploadResult(UploadResult& result, const AppConfig& cfg);

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
  void buildErrorMessage(UploadResult& result);
  void updateCloudTargetCache();
  void updateCloudTargetCacheFor(bool useRelay);
  [[nodiscard]] bool shouldUseRelayForCloudUpload() const;
  [[nodiscard]] bool shouldFallbackToRelay(const UploadResult& result) const;
  [[nodiscard]] bool shouldTreatAsCloudFailure(const UploadResult& result) const;
  void activateRelayFallback();
  void clearRelayFallback();

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
  bool probeServerTimeHeader(bool allowInsecure);

  // performQosTest helpers
  bool executeQosSample(class HTTPClient& http,
                        const char* url,
                        const char* method,
                        const char* payload,
                        bool useOtaToken,
                        const AppConfig& cfg,
                        unsigned long& duration);
  static constexpr unsigned long MAX_BACKOFF_MS = 300000;  // 5 minutes cap (was 1 hour)
  static constexpr unsigned long GATEWAY_MODE_TTL_MS = 30000;
  static constexpr unsigned long RELAY_FALLBACK_PIN_MS = 1800000UL;   // 30 minutes
  static constexpr unsigned long RELAY_RETRY_DELAY_MS = 5000UL;
  DependencyRefs m_deps;
  OperationalState m_runtime;
  TransportRuntime m_transport;
  QosRuntime m_qos;
  ResourceState m_resources;
  GuardPolicy m_policy;
  RuntimeHealth m_health;
  ControllerContext m_context;
};
#endif
