#ifndef OTA_MANAGER_HEALTH_H
#define OTA_MANAGER_HEALTH_H

#include <Arduino.h>

#include "REDACTED"
#include "REDACTED"

namespace OtaManagerHealth {

struct HeapBudget {
  uint32_t freeHeap = 0;
  uint32_t maxBlock = 0;
  uint32_t minTotal = REDACTED
  uint32_t minBlock = 0;
  bool healthy = false;
};

inline HeapBudget captureTlsHeapBudget(const OtaManagerDetail:REDACTED
                                       uint32_t extraBlock = 0,
                                       uint32_t extraTotal = REDACTED
  HeapBudget budget;
  budget.freeHeap = ESP.getFreeHeap();
  budget.maxBlock = ESP.getMaxFreeBlockSize();
  budget.minBlock = policy.tlsMinSafeBlock + extraBlock;
  budget.minTotal = REDACTED
  budget.healthy = (budget.maxBlock >= budget.minBlock) && (budget.freeHeap >= budget.minTotal);
  return budget;
}

inline OtaManagerDetail:REDACTED
                                                            const OtaManagerDetail:REDACTED
  OtaManagerDetail:REDACTED
  health.freeHeap = ESP.getFreeHeap();
  health.maxBlock = ESP.getMaxFreeBlockSize();
  health.wifiConnected = REDACTED
  health.wifiScanBusy = REDACTED
  health.tlsHeapHealthy = captureTlsHeapBudget(policy).healthy;
  health.lastRefreshMs = millis();
  return health;
}

inline void refreshRuntimeHealth(
    OtaManagerDetail:REDACTED
  health = captureRuntimeHealth(wifiManager, policy);
}

}  // namespace OtaManagerHealth

#endif
