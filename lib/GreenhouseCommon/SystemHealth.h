#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <Arduino.h>

#include "constants.h"

// SystemHealth.h
// CPU health monitoring, loop timing, and predictive health scoring.
//
// Provides:
// - Loop timing metrics (avg, max, slow loop detection)
// - Composite health score (0-100)
// - Predictive reboot scheduling
// - Zero-allocation design

namespace SystemHealth {

  // ============================================================================
  // Loop Timing Metrics
  // ============================================================================

  struct LoopMetrics {
    uint32_t loopCount = 0;
    uint32_t slowLoopCount = 0;  // Loops > threshold
    unsigned long totalDurationUs = REDACTED
    unsigned long maxDurationUs = 0;
    unsigned long lastResetTime = 0;

    static constexpr unsigned long SLOW_LOOP_THRESHOLD_US = 50000;  // 50ms

    void reset() {
      loopCount = 0;
      slowLoopCount = 0;
      totalDurationUs = REDACTED
      maxDurationUs = 0;
      lastResetTime = millis();
    }

    void recordLoop(unsigned long durationUs) {
      loopCount++;
      totalDurationUs += REDACTED
      if (durationUs > maxDurationUs)
        maxDurationUs = durationUs;
      if (durationUs > SLOW_LOOP_THRESHOLD_US)
        slowLoopCount++;
    }

    unsigned long getAverageDurationUs() const {
      return loopCount > 0 ? totalDurationUs / loopCount : REDACTED
    }

    uint8_t getSlowLoopPercent() const {
      return loopCount > 0 ? static_cast<uint8_t>((slowLoopCount * 100UL) / loopCount) : 0;
    }

    unsigned long getUptimeSeconds() const {
      return (millis() - lastResetTime) / 1000;
    }
  };

  // ============================================================================
  // Health Score Components (0-100 each)
  // ============================================================================

  struct HealthScore {
    uint8_t heap = 0;           // 100 = lots of heap, 0 = critical
    uint8_t fragmentation = 0;  // 100 = no fragmentation, 0 = severe
    uint8_t cpu = 0;            // 100 = fast loops, 0 = many slow loops
    uint8_t wifi = REDACTED // 100 = strong signal, 0 = disconnected
    uint8_t sensor = 0;         // 100 = all sensors OK, 0 = all failed

    uint8_t overall() const {
      // Weighted average - heap and CPU are more critical
      return static_cast<uint8_t>((heap * 2 + fragmentation + cpu * 2 + wifi + sensor) / 7);
    }

    const char* getGrade() const {
      uint8_t score = overall();
      if (score >= 90)
        return "EXCELLENT";
      if (score >= 75)
        return "GOOD";
      if (score >= 50)
        return "FAIR";
      if (score >= 25)
        return "POOR";
      return "CRITICAL";
    }

    bool needsReboot() const {
      // Reboot if overall is critical OR heap is very low
      return overall() < 20 || heap < 15;
    }
  };

  // ============================================================================
  // Health Calculator (Zero Allocation)
  // ============================================================================

  inline uint8_t calculateHeapScore(uint32_t freeHeap) {
    // 100 = 20KB+, 0 = 2KB or less
    if (freeHeap >= 20000)
      return 100;
    if (freeHeap <= 2000)
      return 0;
    return static_cast<uint8_t>((freeHeap - 2000) * 100 / 18000);
  }

  inline uint8_t calculateFragScore(uint32_t freeHeap, uint32_t maxBlock) {
    if (freeHeap == 0)
      return 0;
    uint8_t fragPercent = static_cast<uint8_t>(100 - (maxBlock * 100UL / freeHeap));
    // 100 = 0% fragmentation, 0 = 80%+ fragmentation
    if (fragPercent >= 80)
      return 0;
    return static_cast<uint8_t>(100 - (fragPercent * 100 / 80));
  }

  inline uint8_t calculateCpuScore(const LoopMetrics& metrics) {
    // 100 = 0% slow loops, 0 = 10%+ slow loops
    uint8_t slowPercent = metrics.getSlowLoopPercent();
    if (slowPercent >= 10)
      return 0;
    return static_cast<uint8_t>(100 - (slowPercent * 10));
  }

  inline uint8_t calculateWifiScore(int32_t rssi) {
    // 100 = > -50 dBm, 0 = < -90 dBm or disconnected
    if (rssi == 0)
      return 0;  // Disconnected
    if (rssi > -50)
      return 100;
    if (rssi < -90)
      return 0;
    return static_cast<uint8_t>((rssi + 90) * 100 / 40);
  }

  inline uint8_t calculateSensorScore(bool shtOk, bool bh1750Ok) {
    if (shtOk && bh1750Ok)
      return 100;
    if (shtOk || bh1750Ok)
      return 50;
    return 0;
  }

  // ============================================================================
  // Global Health State (Singleton Pattern)
  // ============================================================================

  class HealthMonitor {
  public:
    static HealthMonitor& instance() {
      static HealthMonitor inst;
      return inst;
    }

    void init() {
      m_loopMetrics.reset();
      m_lastHealthCheck = millis();
      m_rebootScheduled = false;
      m_loopStartUs = micros();
    }

    void recordLoopTick() {
      unsigned long now = micros();
      unsigned long duration = now - m_loopStartUs;
      m_loopStartUs = now;
      m_loopMetrics.recordLoop(duration);
      m_lastLoopDuration = duration;
    }

    const LoopMetrics& getLoopMetrics() const {
      return m_loopMetrics;
    }
    unsigned long getLastLoopDuration() const {
      return m_lastLoopDuration;
    }

    HealthScore calculateHealth(uint32_t freeHeap, uint32_t maxBlock, int32_t rssi, bool shtOk, bool bh1750Ok) {
      HealthScore score;
      score.heap = calculateHeapScore(freeHeap);
      score.fragmentation = calculateFragScore(freeHeap, maxBlock);
      score.cpu = calculateCpuScore(m_loopMetrics);
      score.wifi = REDACTED
      score.sensor = calculateSensorScore(shtOk, bh1750Ok);
      m_lastScore = score;
      return score;
    }

    const HealthScore& getLastScore() const {
      return m_lastScore;
    }

    bool isRebootScheduled() const {
      return m_rebootScheduled;
    }
    void scheduleReboot() {
      m_rebootScheduled = true;
      m_rebootTime = millis() + 60000;
    }
    bool shouldRebootNow() const {
      return m_rebootScheduled && millis() >= m_rebootTime;
    }

    // Reset metrics periodically to avoid overflow
    void periodicReset() {
      if (m_loopMetrics.loopCount > 100000) {
        m_loopMetrics.reset();
      }
    }

  private:
    HealthMonitor() : m_loopMetrics{}, m_lastScore{} {}

    LoopMetrics m_loopMetrics;
    HealthScore m_lastScore;
    unsigned long m_loopStartUs = 0;
    unsigned long m_lastLoopDuration = 0;
    unsigned long m_lastHealthCheck = 0;
    bool m_rebootScheduled = false;
    unsigned long m_rebootTime = 0;
  };

}  // namespace SystemHealth

#endif  // SYSTEM_HEALTH_H
