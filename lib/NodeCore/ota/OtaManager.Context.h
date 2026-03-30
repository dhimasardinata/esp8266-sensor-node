#ifndef OTA_MANAGER_CONTEXT_H
#define OTA_MANAGER_CONTEXT_H

#include <memory>

#include <WiFiClientSecureBearSSL.h>

#include "config/constants.h"

namespace OtaManagerDetail {

struct ResourceState {
  std::unique_ptr<BearSSL::X509List> localTrustAnchors;
  bool tlsActive = false;
  bool tlsInsecure = false;
  bool tlsFallbackWarned = false;
};

struct GuardPolicy {
  uint32_t tlsMinSafeBlock = AppConstants::TLS_MIN_SAFE_BLOCK_SIZE;
  uint32_t tlsMinTotalHeap = REDACTED
  uint32_t tlsSecureExtraBlock = 2048;
  uint32_t tlsSecureExtraTotal = REDACTED
  unsigned long secureClientTimeoutMs = 15000;
  unsigned long checkHttpTimeoutMs = 15000;
  unsigned long downloadHttpTimeoutMs = 20000;
};

struct RuntimeHealth {
  uint32_t freeHeap = 0;
  uint32_t maxBlock = 0;
  bool wifiConnected = REDACTED
  bool wifiScanBusy = REDACTED
  bool tlsHeapHealthy = false;
  unsigned long lastRefreshMs = 0;
};

}  // namespace OtaManagerDetail

#endif
