#ifndef API_CLIENT_H
#define API_CLIENT_H
#include <Arduino.h>
#include <IntervalTimer.h>

#include "ConfigManager.h"
#include "ICacheManager.h"
#include "ISensorManager.h"

class AsyncWebSocket;
class NtpClient;
class WifiManager;
namespace BearSSL {
  class WiFiClientSecure;
}
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
  int m_sampleCount = 0;
  unsigned long m_lastApiSuccessMillis = 0;
  enum class UploadState { IDLE, UPLOADING, PAUSED };
  UploadState m_uploadState = UploadState::IDLE;
  uint32_t m_consecutiveUploadFailures = 0;
  static constexpr unsigned long MAX_BACKOFF_MS = 3600000;
};
#endif