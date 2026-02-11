#ifndef BOOT_GUARD_H
#define BOOT_GUARD_H

#include <Arduino.h>

class BootGuard {
public:
  // Reason for the last reboot (Persistent in RTC)
  enum class RebootReason : uint32_t {
    UNKNOWN = 0,
    POWER_ON = 1,      // Normal power up
    HW_WDT = 2,        // Hardware Watchdog
    EXCEPTION = 3,     // Exception/Crash
    SOFT_WDT = 4,      // Software Watchdog
    SOFT_RESTART = 5,  // ESP.restart()
    DEEP_SLEEP = 6,    // Wake from sleep
    // Custom Reasons:
    OTA_UPDATE = REDACTED
    FACTORY_RESET = 11,
    HEALTH_CHECK = 12,  // Proactive maintenance
    CONFIG_CHANGE = 13,
    COMMAND = 14  // Remote "reboot" command
  };

  // Store enum as explicit uint32_t to ensure size
  struct alignas(4) RtcData {
    uint32_t magic;
    uint32_t crashCount;
    uint32_t lastReasonRaw;  // Store RebootReason as uint32_t
    uint32_t lastCrashTime;  // FIX: Add timestamp for crash rate detection
    uint32_t crc;
  };
  static_assert(sizeof(RtcData) == 20, "RtcData layout must remain stable");

  static void markStable();
  static void incrementCrashCount();
  static void clear();
  static uint32_t getCrashCount();

  static void setRebootReason(RebootReason reason);
  static RebootReason getLastRebootReason();

private:
  static RtcData data;
  static bool read();
  static void write();
  static uint32_t calculateCRC(const RtcData& d);
  static bool isValidReason(RebootReason reason);
};

#endif  // BOOT_GUARD_H
