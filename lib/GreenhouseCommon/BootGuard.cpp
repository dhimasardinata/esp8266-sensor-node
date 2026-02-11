#include "BootGuard.h"

#include <string.h>
#include <user_interface.h>  // Required for rst_info

#include "Logger.h"

#define RTC_MAGIC 0xDEADCAFE

// FIX: Move to safer RTC block (away from WiFi and ArduinoOTA)
// RTC Memory Map:
// Blocks 0-31:   Reserved by WiFi/System
// Blocks 32-63:  Available for user
// Block 64:      Used by ArduinoOTA (if enabled)
// Block 96:      BootGuard (THIS MODULE) - SAFE ZONE
#define RTC_BLOCK_OFFSET 96

// FIX: Maximum crash count to prevent overflow
#define MAX_CRASH_COUNT 999

// FIX: Rapid crash threshold (milliseconds)
#define RAPID_CRASH_THRESHOLD_MS 5000

// Static member definition
BootGuard::RtcData BootGuard::data;

namespace {
  // Helper to detect if the last reset was a crash
  bool isCrashReset() {
    struct rst_info* rst = system_get_rst_info();
    if (!rst)
      return false;
    return (rst->reason == REASON_WDT_RST || rst->reason == REASON_EXCEPTION_RST || rst->reason == REASON_SOFT_WDT_RST);
  }

  // CRC32 Helper (IEEE 802.3 Polynomial - 0xEDB88320)
  uint32_t internal_crc32(const void* d, size_t len, uint32_t crc = 0) {
    const uint8_t* p = (const uint8_t*)d;
    crc = ~crc;
    while (len--) {
      crc ^= *p++;
      for (int i = 0; i < 8; i++) {
        crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
      }
    }
    return ~crc;
  }
}  // namespace

uint32_t BootGuard::calculateCRC(const RtcData& d) {
  // Calculate CRC over all fields except the CRC itself
  uint32_t crc = 0;
  crc = internal_crc32(&d.magic, sizeof(d.magic), crc);
  crc = internal_crc32(&d.crashCount, sizeof(d.crashCount), crc);
  crc = internal_crc32(&d.lastReasonRaw, sizeof(d.lastReasonRaw), crc);
  crc = internal_crc32(&d.lastCrashTime, sizeof(d.lastCrashTime), crc);
  return crc;
}

bool BootGuard::read() {
  system_rtc_mem_read(RTC_BLOCK_OFFSET, &data, sizeof(data));

  // Verify Magic
  if (data.magic != RTC_MAGIC) {
    LOG_WARN("BOOT", F("RTC Magic mismatch (expected 0x%08X, got 0x%08X)"), RTC_MAGIC, data.magic);
    return false;
  }

  // Verify CRC
  uint32_t calculatedCrc = calculateCRC(data);
  if (calculatedCrc != data.crc) {
    LOG_WARN("BOOT", F("RTC CRC mismatch (expected 0x%08X, got 0x%08X)"), calculatedCrc, data.crc);
    return false;
  }

  return true;
}

void BootGuard::write() {
  data.magic = RTC_MAGIC;
  data.crc = calculateCRC(data);
  system_rtc_mem_write(RTC_BLOCK_OFFSET, &data, sizeof(data));
}

bool BootGuard::isValidReason(RebootReason reason) {
  uint32_t raw = static_cast<uint32_t>(reason);
  // Valid reasons: 0-6, 10-14
  return (raw <= 6) || (raw >= 10 && raw <= 14);
}

void BootGuard::incrementCrashCount() {
  bool wasCleared = false;

  if (!read()) {
    // Cold boot or corrupted RTC: Initialize with defaults
    LOG_WARN("BOOT", F("RTC data invalid or corrupt - initializing fresh"));
    clear();
    wasCleared = true;
  }

  // Check hardware reset reason
  if (isCrashReset()) {
    uint32_t now = millis();

    // FIX: Handle first crash after clear specially
    if (wasCleared) {
      data.crashCount = 1;
      LOG_ERROR("BOOT", F("CRASH detected (first after RTC clear)"));
    } else {
      // FIX: Detect rapid crashes and increment more aggressively
      bool rapidCrash = (data.lastCrashTime != 0) && ((now - data.lastCrashTime) < RAPID_CRASH_THRESHOLD_MS);

      if (rapidCrash) {
        LOG_ERROR("BOOT", F("RAPID CRASH detected (<%u ms since last)"), RAPID_CRASH_THRESHOLD_MS);

        // FIX: Bounds check to prevent overflow
        if (data.crashCount < MAX_CRASH_COUNT - 1) {
          data.crashCount += 2;  // Double penalty for rapid crashes
        } else {
          data.crashCount = MAX_CRASH_COUNT;
        }
      } else {
        // Normal crash
        // FIX: Bounds check to prevent overflow
        if (data.crashCount < MAX_CRASH_COUNT) {
          data.crashCount++;
        } else {
          data.crashCount = MAX_CRASH_COUNT;
        }
      }

      LOG_ERROR("BOOT", F("CRASH #%u detected"), data.crashCount);
    }

    data.lastCrashTime = now;
    data.lastReasonRaw = static_cast<uint32_t>(RebootReason::EXCEPTION);

  } else {
    // Normal boot or manual reset - preserve counter, update reason
    struct rst_info* rst = system_get_rst_info();

    if (rst && rst->reason == REASON_DEEP_SLEEP_AWAKE) {
      data.lastReasonRaw = static_cast<uint32_t>(RebootReason::DEEP_SLEEP);
      LOG_INFO("BOOT", F("Wake from deep sleep"));
    } else if (rst && rst->reason == REASON_DEFAULT_RST) {
      data.lastReasonRaw = static_cast<uint32_t>(RebootReason::POWER_ON);
      LOG_INFO("BOOT", F("Power-on reset"));
    } else if (rst && rst->reason == REASON_SOFT_RESTART) {
      data.lastReasonRaw = static_cast<uint32_t>(RebootReason::SOFT_RESTART);
      LOG_INFO("BOOT", F("Software restart"));
    }

    // Log current state
    if (data.crashCount > 0) {
      LOG_WARN("BOOT", F("Normal boot with pending crash count: %u"), data.crashCount);
    }
  }

  write();
}

void BootGuard::setRebootReason(BootGuard::RebootReason reason) {
  // FIX: Validate enum value
  if (!isValidReason(reason)) {
    LOG_ERROR("BOOT", F("Invalid reboot reason: %u - using UNKNOWN"), static_cast<uint32_t>(reason));
    reason = RebootReason::UNKNOWN;
  }

  if (!read()) {
    clear();
  }

  data.lastReasonRaw = static_cast<uint32_t>(reason);
  write();

  LOG_INFO("BOOT", F("Reboot reason set: %u"), static_cast<uint32_t>(reason));
}

BootGuard::RebootReason BootGuard::getLastRebootReason() {
  if (read()) {
    // FIX: Validate before casting back to enum
    if (isValidReason(static_cast<RebootReason>(data.lastReasonRaw))) {
      return static_cast<RebootReason>(data.lastReasonRaw);
    } else {
      LOG_WARN("BOOT", F("Invalid stored reason: %u"), data.lastReasonRaw);
      return RebootReason::UNKNOWN;
    }
  }
  return RebootReason::UNKNOWN;
}

uint32_t BootGuard::getCrashCount() {
  if (read()) {
    // FIX: Sanity check in case of corruption
    if (data.crashCount > MAX_CRASH_COUNT) {
      LOG_ERROR("BOOT", F("Crash count corrupt: %u - capping to %u"), data.crashCount, MAX_CRASH_COUNT);
      data.crashCount = MAX_CRASH_COUNT;
      write();
    }
    return data.crashCount;
  }
  return 0;
}

void BootGuard::markStable() {
  if (read()) {
    if (data.crashCount > 0) {
      uint32_t oldCount = data.crashCount;

      data.crashCount = 0;
      data.lastCrashTime = 0;                                              // FIX: Also clear timestamp
      data.lastReasonRaw = static_cast<uint32_t>(RebootReason::POWER_ON);  // FIX: Reset reason to normal

      write();

      LOG_INFO("BOOT", F("System stable (>60s). Crash counter cleared (was: %u)."), oldCount);
    }
  }
}

void BootGuard::clear() {
  // FIX: Zero entire struct including any potential padding
  memset(&data, 0, sizeof(data));

  data.magic = RTC_MAGIC;
  data.crashCount = 0;
  data.lastReasonRaw = static_cast<uint32_t>(RebootReason::UNKNOWN);
  data.lastCrashTime = 0;

  write();

  LOG_INFO("BOOT", F("RTC data cleared and initialized"));
}