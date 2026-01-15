// src/HAL.h
#ifndef HAL_H
#define HAL_H

#include <Arduino.h>
#include <LittleFS.h>
#include "Logger.h"

// This class initializes Serial communication on construction.
class SerialManager {
public:
  SerialManager() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
    }
    LOG_INFO("HAL", F("--- Serial Initialized ---"));
  }
  // Prevent copying
  SerialManager(const SerialManager&) = delete;
  SerialManager& operator=(const SerialManager&) = delete;
};

// This class initializes and mounts the filesystem on construction.
class FileSystemManager {
public:
  FileSystemManager() {
    if (tryMountWithRetry()) {
      LOG_INFO("FS", F("--- FileSystem Initialized (LittleFS) ---"));
    } else {
      handleMountFailure();
    }
  }
  // Prevent copying
  FileSystemManager(const FileSystemManager&) = delete;
  FileSystemManager& operator=(const FileSystemManager&) = delete;

private:
  // Retry mount up to 3 times to handle brownout false positives
  static bool tryMountWithRetry() {
    for (int i = 0; i < 3; i++) {
      if (LittleFS.begin()) return true;
      LOG_WARN("FS", F("Mount failed, retrying in 500ms..."));
      delay(500);
    }
    return false;
  }

  static void handleMountFailure() {
    LOG_ERROR("FS", F("Mount failed after retries. Formatting Filesystem..."));
    LittleFS.format();
    if (!LittleFS.begin()) {
      LOG_ERROR("FS", F("CRITICAL: LittleFS Mount Failed even after Format. Hardware Error?"));
    } else {
      LOG_INFO("FS", F("Formatted and Mounted."));
    }
  }
};

#endif