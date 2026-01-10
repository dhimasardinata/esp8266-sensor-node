#include "BootGuard.h"

#include <user_interface.h>  // Required for rst_info
#include "Logger.h"

#define RTC_MAGIC 0xDEADCAFE
#define RTC_BLOCK_OFFSET 65

struct RtcData {
  uint32_t magic;
  uint32_t crashCount;
};

bool BootGuard::check() {
  RtcData data;
  system_rtc_mem_read(RTC_BLOCK_OFFSET, &data, sizeof(data));

  // --- NEW LOGIC START ---
  struct rst_info* rst = system_get_rst_info();
  bool isCrash = false;

  if (rst) {
    // Only count these as actual crashes
    if (rst->reason == REASON_WDT_RST || rst->reason == REASON_EXCEPTION_RST || rst->reason == REASON_SOFT_WDT_RST) {
      isCrash = true;
    }
  }
  // --- NEW LOGIC END ---

  if (data.magic != RTC_MAGIC) {
    // Cold start (power loss) -> Reset everything
    data.magic = RTC_MAGIC;
    data.crashCount = 0;
  } else {
    if (isCrash) {
      // Only increment if it was an actual crash
      data.crashCount++;
    } else {
      // If it was a manual reboot or OTA, KEEP the magic but DON'T increment
      // Optionally: data.crashCount = 0; // Uncomment if you want manual reboots to clear the score
    }
  }

  system_rtc_mem_write(RTC_BLOCK_OFFSET, &data, sizeof(data));

  if (data.crashCount > 0) {
    LOG_WARN("BOOT", F("Consecutive Crash Count: %u"), data.crashCount);
  }

  return true;
}

uint32_t BootGuard::getCrashCount() {
  RtcData data;
  system_rtc_mem_read(RTC_BLOCK_OFFSET, &data, sizeof(data));
  if (data.magic == RTC_MAGIC) {
    return data.crashCount;
  }
  return 0;
}

void BootGuard::markStable() {
  RtcData data;
  system_rtc_mem_read(RTC_BLOCK_OFFSET, &data, sizeof(data));

  // Hanya tulis ke RTC jika counter belum 0 (untuk menghemat cycle tulis)
  if (data.crashCount > 0) {
    data.magic = RTC_MAGIC;
    data.crashCount = 0;  // RESET KE 0 KARENA SUDAH STABIL
    system_rtc_mem_write(RTC_BLOCK_OFFSET, &data, sizeof(data));
    LOG_INFO("BOOT", F("System stable (>60s). Crash counter reset to 0."));
  }
}

void BootGuard::clear() {
  RtcData data = {0, 0};
  system_rtc_mem_write(RTC_BLOCK_OFFSET, &data, sizeof(data));
}