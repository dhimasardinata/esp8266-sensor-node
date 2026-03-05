#include "Logger.h"

#include <cstdarg>

// Default to DEBUG level
LogLevel Logger::currentLevel = LogLevel::DEBUG;

void Logger::setLevel(LogLevel level) noexcept {
  currentLevel = level;
}

const char* Logger::levelToString(LogLevel level) noexcept {
  // PROGMEM string table for level names
  static const char L_DEBUG[] PROGMEM = "DEBUG";
  static const char L_INFO[] PROGMEM = "INFO";
  static const char L_WARN[] PROGMEM = "WARN";
  static const char L_ERROR[] PROGMEM = "ERROR";
  static const char L_NONE[] PROGMEM = "NONE";
  static const char* const names[] PROGMEM = {L_DEBUG, L_INFO, L_WARN, L_ERROR, L_NONE};

  uint8_t idx = static_cast<uint8_t>(level);
  return (idx < 5) ? (const char*)pgm_read_ptr(&names[idx]) : "?";
}

// =============================================================================
// Shared Helper Functions (Eliminate Duplication)
// =============================================================================

// Check if log should be output based on level threshold
static inline bool shouldLog(LogLevel level) {
  return static_cast<uint8_t>(level) >= static_cast<uint8_t>(Logger::currentLevel);
}

// Print log prefix: [LEVEL][TAG]
static inline void printLogPrefix(LogLevel level, const char* tag) {
  Serial.printf_P(PSTR("[%S][%s] "), reinterpret_cast<PGM_P>(Logger::levelToString(level)), tag);
}

// =============================================================================
// Main Log Functions
// =============================================================================

void Logger::log(LogLevel level, const char* tag, const char* fmt, ...) {
  if (!shouldLog(level))
    return;

  printLogPrefix(level, tag);

  va_list args;
  va_start(args, fmt);
  char buffer[160];
  vsnprintf(buffer, sizeof(buffer), fmt, args);  // RAM format string
  buffer[sizeof(buffer) - 1] = '\0';             // Hard safety guarantee
  va_end(args);

  Serial.println(buffer);
}

void Logger::log(LogLevel level, const char* tag, const __FlashStringHelper* fmt, ...) {
  if (!shouldLog(level))
    return;

  printLogPrefix(level, tag);

  va_list args;
  va_start(args, fmt);
  char buffer[160];
  vsnprintf_P(buffer, sizeof(buffer), (const char*)fmt, args);  // Flash format string
  buffer[sizeof(buffer) - 1] = '\0';                            // Safety: Hard safety guarantee for null-termination
  va_end(args);

  Serial.println(buffer);
}
