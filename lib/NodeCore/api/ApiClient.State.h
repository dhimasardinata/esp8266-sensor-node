#ifndef API_CLIENT_STATE_H
#define API_CLIENT_STATE_H

#include <array>
#include <cstdint>
#include <memory>

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <system/IntervalTimer.h>

#include "system/ConfigManager.h"

enum class UploadMode : uint8_t;
class AsyncWebSocket;
class NtpClient;
class WifiManager;
class CacheManager;
class SensorManager;
namespace BearSSL {
  class WiFiClientSecure;
  class X509List;
}

struct UploadResult {
  int16_t httpCode = 0;
  uint8_t success = 0;
  char message[32] = {0};
};

namespace ApiClientDetail {

enum class UploadRecordSource : uint8_t { NONE, RTC, LITTLEFS };
enum class UploadRecordLoad : uint8_t { READY, EMPTY, RETRY, FATAL };

struct EmergencyRecord {
  uint32_t timestamp = 0;
  int16_t temp10 = 0;
  int16_t hum10 = 0;
  uint16_t lux = 0;
  int16_t rssi = 0;
};

enum class EmergencyQueueReason : uint8_t {
  STATE,
  DRAINED,
  BACKPRESSURE_OFF,
  BACKPRESSURE_HOLD,
  ENQUEUE,
  QUEUE_FULL
};

static constexpr uint8_t kEmergencyQueueCapacity = 16;

struct RoutingState {
  UploadMode uploadMode{};
  UplinkMode uplinkMode = UplinkMode::AUTO;
  bool localGatewayMode = false;
  bool targetIsEdge = false;
  bool cloudTargetIsRelay = false;
  bool forceRelayNextCloudAttempt = false;
  UploadRecordSource loadedRecordSource = UploadRecordSource::NONE;
  unsigned long lastCloudRetryAttempt = 0;
  unsigned long relayPinnedUntil = 0;
  int8_t cachedGatewayMode = -1;
  unsigned long lastGatewayModeCheck = 0;
  bool currentRecordSentToGateway = false;
  bool forceCloudAfterEdgeFailure = false;
};

struct QueueState {
  uint8_t popFailStreak = 0;
  unsigned long popRetryAfter = 0;
  uint8_t rtcFallbackFsFailStreak = 0;
  unsigned long rtcFallbackFsRetryAfter = 0;
  std::array<EmergencyRecord, kEmergencyQueueCapacity> emergencyQueue{};
  uint8_t emergencyHead = 0;
  uint8_t emergencyTail = 0;
  uint8_t emergencyCount = 0;
  bool emergencyBackpressure = false;
  EmergencyRecord pendingLiveSnapshot{};
  bool liveSnapshotPending = false;
  bool liveSnapshotInFlight = false;
  unsigned long lastEmergencyLogMs = 0;
};

struct ImmediateUploadState {
  bool requested = false;
  uint8_t warmup = 0;
  unsigned long lastDeferLog = 0;
  unsigned long retryAt = 0;
  int8_t gatewayMode = -2;
  bool pollReady = false;
  bool restoreWsAfter = false;
};

enum class QueuedUploadTargetDecision : uint8_t { PROCEED, HOLD, WAIT };
enum class UploadState { IDLE, UPLOADING, PAUSED };
enum class HttpState { IDLE, CONNECTING, SENDING_REQUEST, WAITING_RESPONSE, READING_RESPONSE, COMPLETE, FAILED };
enum class QosTaskType { NONE, UPLOAD, OTA };

struct QosBuffers {
  char url[160] = {0};
  char method[8] = {0};
  char payload[64] = {0};
};

struct DependencyRefs {
  AsyncWebSocket& ws;
  NtpClient& ntpClient;
  WifiManager& wifiManager;
  SensorManager& sensorManager;
  BearSSL::WiFiClientSecure& secureClient;
  ConfigManager& configManager;
  CacheManager& cacheManager;
  const BearSSL::X509List* trustAnchors = nullptr;
};

struct OperationalState {
  IntervalTimer dataCreationTimer;
  IntervalTimer sampleTimer;
  IntervalTimer cacheSendTimer;
  IntervalTimer cacheFlushTimer;
  IntervalTimer swWdtTimer;
  int32_t rssiSum = 0;
  uint16_t sampleCount = 0;
  unsigned long lastApiSuccessMillis = 0;
  UploadState uploadState = UploadState::IDLE;
  uint32_t consecutiveUploadFailures = 0;
  unsigned long lastTimeProbe = 0;
  RoutingState route;
  QueueState queue;
  ImmediateUploadState immediate;
  bool isSystemPaused = false;
  uint8_t lowMemCounter = 0;
  bool otaInProgress = REDACTED
};

struct TransportRuntime {
  char cloudHost[48] = {0};
  char cloudPath[48] = {0};
  char edgeHost[48] = {0};
  HttpState httpState = HttpState::IDLE;
  unsigned long stateEntryTime = 0;
  size_t payloadLen = 0;
  UploadResult lastResult;
  WiFiClient* activeClient = REDACTED
  char lastResponseLocation[64] = {0};
  std::unique_ptr<HTTPClient> httpClient;
  WiFiClient plainClient;
};

struct QosRuntime {
  QosTaskType pendingTask = QosTaskType::NONE;
  bool active = false;
  uint8_t sampleIdx = 0;
  unsigned long nextAt = 0;
  int successCount = 0;
  unsigned long totalDuration = REDACTED
  unsigned long minLat = 0;
  unsigned long maxLat = 0;
  const char* targetName = nullptr;
  bool usesOtaToken = REDACTED
  std::unique_ptr<QosBuffers> buffers;
};

}  // namespace ApiClientDetail

#endif
