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
    // FIX: Retry mechanism. Do not format immediately on first failure.
    // False positives happen during brownouts.
    bool mounted = false;
    for (int i = 0; i < 3; i++) {
      if (LittleFS.begin()) {
        mounted = true;
        break;
      }
      LOG_WARN("FS", F("Mount failed, retrying in 500ms..."));
      delay(500);
    }

    if (!mounted) {
      LOG_ERROR("FS", F("Mount failed after retries. Formatting Filesystem..."));
      LittleFS.format();
      if (!LittleFS.begin()) {
        LOG_ERROR("FS", F("CRITICAL: LittleFS Mount Failed even after Format. Hardware Error?"));
        // Don't halt completely, allow Rescue Mode to potentially run in main.cpp
      } else {
        LOG_INFO("FS", F("Formatted and Mounted."));
      }
    } else {
      LOG_INFO("FS", F("--- FileSystem Initialized (LittleFS) ---"));
    }
  }
  // Prevent copying
  FileSystemManager(const FileSystemManager&) = delete;
  FileSystemManager& operator=(const FileSystemManager&) = delete;
};

#endif