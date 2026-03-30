#ifndef API_CLIENT_HEALTH_H
#define API_CLIENT_HEALTH_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>

#include "api/ApiClient.Context.h"
#include "storage/CacheManager.h"
#include "storage/RtcManager.h"
#include "REDACTED"
#include "config/constants.h"

namespace ApiClientHealth {

struct HeapBudget {
  uint32_t freeHeap = 0;
  uint32_t maxBlock = 0;
  uint32_t minTotal = REDACTED
  uint32_t minBlock = 0;
  bool healthy = false;
};

inline HeapBudget captureTlsHeapBudget(const ApiClientDetail::ControllerContext& ctx,
                                       uint32_t extraBlock = 0,
                                       uint32_t extraTotal = REDACTED
  HeapBudget budget;
  budget.freeHeap = ESP.getFreeHeap();
  budget.maxBlock = ESP.getMaxFreeBlockSize();
  budget.minBlock = ctx.policy.tlsMinSafeBlock + extraBlock;
  budget.minTotal = REDACTED
  if (ctx.deps.ws.count() > 0) {
    budget.minBlock += ctx.policy.tlsWsMarginBlock;
    budget.minTotal += REDACTED
  }
  budget.healthy = (budget.maxBlock >= budget.minBlock) && (budget.freeHeap >= budget.minTotal);
  return budget;
}

inline HeapBudget captureApiHeapBudget(const ApiClientDetail::ControllerContext& ctx) {
  HeapBudget budget;
  budget.freeHeap = ESP.getFreeHeap();
  budget.maxBlock = ESP.getMaxFreeBlockSize();
  budget.minBlock = ctx.policy.apiMinSafeBlock;
  budget.minTotal = REDACTED
  budget.healthy = (budget.maxBlock >= budget.minBlock) && (budget.freeHeap >= budget.minTotal);
  return budget;
}

inline ApiClientDetail::RuntimeHealth captureRuntimeHealth(const ApiClientDetail::ControllerContext& ctx) {
  ApiClientDetail::RuntimeHealth health;
  health.freeHeap = ESP.getFreeHeap();
  health.maxBlock = ESP.getMaxFreeBlockSize();
  health.wifiConnected = REDACTED
  health.wifiScanBusy = REDACTED
  health.rtcHasCapacity = !RtcManager::isFull();
  health.littleFsHasCapacity = (ctx.deps.cacheManager.get_size() < MAX_CACHE_DATA_SIZE);
  health.storageBackpressure = ctx.runtime.queue.emergencyBackpressure;
  health.tlsHeapHealthy = captureTlsHeapBudget(ctx).healthy;
  health.lastRefreshMs = millis();
  return health;
}

inline void refreshRuntimeHealth(ApiClientDetail::ControllerContext& ctx) {
  ctx.health = captureRuntimeHealth(ctx);
}

}  // namespace ApiClientHealth

#endif
