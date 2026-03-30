#ifndef MEMORY_TELEMETRY_H
#define MEMORY_TELEMETRY_H

#include <Arduino.h>

#include "system/Logger.h"

namespace MemoryTelemetry {

  struct HeapSnapshot {
    uint32_t freeHeap = 0;
    uint32_t maxBlock = 0;
    uint8_t fragmentation = 0;

    static HeapSnapshot capture() {
      HeapSnapshot snapshot;
      snapshot.freeHeap = ESP.getFreeHeap();
      snapshot.maxBlock = ESP.getMaxFreeBlockSize();
      snapshot.fragmentation = ESP.getHeapFragmentation();
      return snapshot;
    }
  };

  struct HeapDelta {
    int32_t freeHeapBytes = 0;
    int32_t maxBlockBytes = 0;
    int32_t freeHeapPercentTenths = 0;
    int32_t maxBlockPercentTenths = 0;
  };

  struct HeapDeltaSummary {
    HeapDelta delta;
    char freeHeapPercent[16] = {0};
    char maxBlockPercent[16] = {0};
  };

  inline int32_t calculatePercentTenths(uint32_t before, int32_t deltaBytes) {
    if (before == 0) {
      return 0;
    }
    return static_cast<int32_t>((static_cast<int64_t>(deltaBytes) * 1000LL) / static_cast<int64_t>(before));
  }

  inline HeapDelta compare(const HeapSnapshot& before, const HeapSnapshot& after) {
    HeapDelta delta;
    delta.freeHeapBytes = static_cast<int32_t>(after.freeHeap) - static_cast<int32_t>(before.freeHeap);
    delta.maxBlockBytes = static_cast<int32_t>(after.maxBlock) - static_cast<int32_t>(before.maxBlock);
    delta.freeHeapPercentTenths = calculatePercentTenths(before.freeHeap, delta.freeHeapBytes);
    delta.maxBlockPercentTenths = calculatePercentTenths(before.maxBlock, delta.maxBlockBytes);
    return delta;
  }

  inline uint32_t positiveBytes(int32_t deltaBytes) {
    return (deltaBytes > 0) ? static_cast<uint32_t>(deltaBytes) : 0;
  }

  inline void formatSignedPercentTenths(char* out, size_t out_len, int32_t percentTenths) {
    if (!out || out_len == 0) {
      return;
    }
    const char sign = (percentTenths >= 0) ? '+' : '-';
    const uint32_t absTenths =
        (percentTenths >= 0) ? static_cast<uint32_t>(percentTenths) : static_cast<uint32_t>(-percentTenths);
    const int written = snprintf_P(out,
                                   out_len,
                                   PSTR("%c%lu.%01lu%%"),
                                   sign,
                                   static_cast<unsigned long>(absTenths / 10U),
                                   static_cast<unsigned long>(absTenths % 10U));
    if (written < 0) {
      out[0] = '\0';
      return;
    }
    out[out_len - 1] = '\0';
  }

  inline HeapDeltaSummary summarize(const HeapSnapshot& before, const HeapSnapshot& after) {
    HeapDeltaSummary summary;
    summary.delta = compare(before, after);
    formatSignedPercentTenths(summary.freeHeapPercent,
                              sizeof(summary.freeHeapPercent),
                              summary.delta.freeHeapPercentTenths);
    formatSignedPercentTenths(summary.maxBlockPercent,
                              sizeof(summary.maxBlockPercent),
                              summary.delta.maxBlockPercentTenths);
    return summary;
  }

  inline void logReleaseSummary(const char* tag, const char* label, const HeapSnapshot& before, const HeapSnapshot& after) {
    const HeapDeltaSummary summary = summarize(before, after);
    LOG_INFO(tag,
             "%s: free %u->%u (%ld B, %s), block %u->%u (%ld B, %s), frag %u%%->%u%%, freed=%u B",
             label,
             before.freeHeap,
             after.freeHeap,
             static_cast<long>(summary.delta.freeHeapBytes),
             summary.freeHeapPercent,
             before.maxBlock,
             after.maxBlock,
             static_cast<long>(summary.delta.maxBlockBytes),
             summary.maxBlockPercent,
             before.fragmentation,
             after.fragmentation,
             positiveBytes(summary.delta.freeHeapBytes));
  }

}  // namespace MemoryTelemetry

#endif  // MEMORY_TELEMETRY_H
