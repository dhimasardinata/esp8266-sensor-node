#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#include <Arduino.h>

class CrashHandler {
public:
  static void process();
  static String getLog();
  static void clearLog();
  static bool hasCrashLog();
};

#endif  // CRASH_HANDLER_H