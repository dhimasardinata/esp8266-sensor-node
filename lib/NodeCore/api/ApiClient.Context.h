#ifndef API_CLIENT_CONTEXT_H
#define API_CLIENT_CONTEXT_H

#include <array>
#include <memory>

#include <WiFiClientSecureBearSSL.h>

#include "api/ApiClient.State.h"
#include "system/ConfigManager.h"
#include "config/constants.h"

namespace ApiClientDetail {

using PayloadBuffer = std::array<char, MAX_PAYLOAD_SIZE + 1>;

struct ResourceState {
  std::unique_ptr<PayloadBuffer> sharedBuffer;
  std::unique_ptr<BearSSL::X509List> localTrustAnchors;
  bool tlsActive = false;
  bool tlsInsecure = false;
  bool tlsFallbackWarned = false;
};

struct GuardPolicy {
  uint32_t tlsMinSafeBlock = AppConstants::TLS_MIN_SAFE_BLOCK_SIZE;
  uint32_t tlsMinTotalHeap = REDACTED
  uint32_t tlsWsMarginBlock = 512;
  uint32_t tlsWsMarginTotal = REDACTED
  uint32_t tlsSecureExtraBlock = 2048;
  uint32_t tlsSecureExtraTotal = REDACTED
  uint32_t apiMinSafeBlock = AppConstants::API_MIN_SAFE_BLOCK_SIZE;
  uint32_t apiMinTotalHeap = REDACTED
  unsigned long edgeHttpTimeoutMs = 2000;
  unsigned long connectTimeoutMs = 5000;
  unsigned long secureLineTimeoutMs = 5000;
  unsigned long writeTimeoutMs = 3000;
  unsigned long waitResponseTimeoutMs = 10000;
  unsigned long previewTimeoutMs = 1500;
  unsigned long secureClientTimeoutMs = 15000;
  unsigned long emergencyLogThrottleMs = 5000;
  unsigned long littleFsFallbackCooldownBaseMs = 5000;
  unsigned long littleFsFallbackCooldownMaxMs = 60000;
};

struct RuntimeHealth {
  uint32_t freeHeap = 0;
  uint32_t maxBlock = 0;
  bool wifiConnected = REDACTED
  bool wifiScanBusy = REDACTED
  bool tlsHeapHealthy = false;
  bool rtcHasCapacity = true;
  bool littleFsHasCapacity = true;
  bool storageBackpressure = false;
  unsigned long lastRefreshMs = 0;
};

struct ControllerContext {
  DependencyRefs& deps;
  OperationalState& runtime;
  TransportRuntime& transport;
  QosRuntime& qos;
  ResourceState& resources;
  GuardPolicy& policy;
  RuntimeHealth& health;
};

}  // namespace ApiClientDetail

#endif
