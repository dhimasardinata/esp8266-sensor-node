#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#include <Arduino.h>

class CrashHandler {
public:
  static void process();
  [[deprecated("Use streamLogTo to avoid heap allocations")]] static String getLog();
  static size_t streamLogTo(Print& output, size_t maxBytes = 0);  // Streams without heap allocation
  static void clearLog();
  static bool hasCrashLog();
};

#endif  // CRASH_HANDLER_H
