#include "BootManager.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>

#include "BootGuard.h"
#include "CrashHandler.h"
#include "HAL.h"
#include "Logger.h"

namespace {
  bool formatLittleFsSafe() {
    ESP.wdtDisable();
    bool ok = LittleFS.format();
    ESP.wdtEnable(8000);
    return ok;
  }
}  // namespace

void BootManager::run() {
  // Factory Reset requested via Portal/Command.
  // Keep this before filesystem mount (mirrors previous setup ordering).
  if (BootGuard::getLastRebootReason() == BootGuard::RebootReason::FACTORY_RESET) {
    LOG_WARN("BOOT", F("Factory Reset Flag Detected!"));
    LOG_WARN("BOOT", F("Formatting Filesystem (This may take 30s)..."));

    // Feed WDT immediately before start
    ESP.wdtFeed();

    // Format
    if (formatLittleFsSafe()) {
      LOG_INFO("BOOT", F("Format Complete."));
    } else {
      LOG_ERROR("BOOT", F("Format Failed!"));
    }

    // Clear flag to prevent loop
    BootGuard::clear();
    BootGuard::setRebootReason(BootGuard::RebootReason::POWER_ON);

    // Fresh reboot ensures clean state and re-init of config
    LOG_INFO("BOOT", F("Rebooting fresh..."));
    ESP.wdtFeed();
    delay(100);
    ESP.restart();
  }

  // Crash counter increment (BootGuard::check() is internal/deprecated).
  BootGuard::incrementCrashCount();
  uint32_t crashes = BootGuard::getCrashCount();

  FileSystemManager fileSystem;
  (void)fileSystem;
  CrashHandler::process();

  // --- SELF-HEALING LOGIC ---
  // Don't clear counter until stable boot confirmed (handled in main loop).
  if (crashes >= 4 && crashes <= 7) {
    LOG_WARN("AUTO-FIX", F("Level 1 (Attempt %u): Clearing Sensor Cache..."), crashes);

    if (LittleFS.remove("/cache.dat")) {
      LOG_INFO("AUTO-FIX", F("Cache cleared successfully"));
    } else {
      LOG_WARN("AUTO-FIX", F("Cache file not found or already deleted"));
    }

    ESP.wdtFeed();
    delay(100);
    ESP.restart();

  } else if (crashes >= 8 && crashes <= 12) {
    LOG_ERROR("AUTO-FIX", F("Level 2 (Attempt %u): FORMATTING FILESYSTEM..."), crashes);
    ESP.wdtFeed();

    if (formatLittleFsSafe()) {
      LOG_INFO("AUTO-FIX", F("Format Success. Restarting to apply Factory Defaults."));
    } else {
      LOG_ERROR("AUTO-FIX", F("Format Failed! Hardware Issue?"));
    }

    ESP.wdtFeed();
    delay(1000);
    ESP.restart();

  } else if (crashes > 12) {
    LOG_ERROR("AUTO-FIX", F("Level 3 (Attempt %u): System Unstable. Cooling down..."), crashes);
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();

    for (int i = 0; i < 10; i++) {
      delay(1000);
      ESP.wdtFeed();
    }

    LOG_INFO("AUTO-FIX", F("Retrying boot..."));

    if (crashes > 15) {
      LOG_WARN("AUTO-FIX", F("Too many crashes - resetting counter"));
      BootGuard::clear();
    }

    ESP.wdtFeed();
    ESP.restart();
  }
}
