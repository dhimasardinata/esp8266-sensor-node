#include "Logger.h"

#include <cstdarg>

// Default to INFO level
LogLevel Logger::currentLevel = LogLevel::INFO;

void Logger::setLevel(LogLevel level) noexcept {
  currentLevel = level;
}

const char* Logger::levelToString(LogLevel level) noexcept {
  switch (level) {
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO:  return "INFO";
    case LogLevel::WARN:  return "WARN";
    case LogLevel::ERROR: return "ERROR";
    case LogLevel::NONE:  return "NONE";
    default:              return "?";
  }
}

void Logger::log(LogLevel level, const char* tag, const char* fmt, ...) {
  // Runtime level check - skip if below threshold
  if (static_cast<uint8_t>(level) < static_cast<uint8_t>(currentLevel)) {
    return;
  }

  // Format: [LEVEL][TAG] message
  // Optimize: Use PSTR to store format string in Flash
  Serial.printf_P(PSTR("[%s][%s] "), levelToString(level), tag);

  // Handle variadic arguments
  va_list args;
  va_start(args, fmt);
  
  // Use a stack buffer for formatting (avoid heap allocation)
  char buffer[128];
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  
  va_end(args);
  
  Serial.println(buffer);
}

void Logger::log(LogLevel level, const char* tag, const __FlashStringHelper* fmt, ...) {
  // Runtime level check - skip if below threshold
  if (static_cast<uint8_t>(level) < static_cast<uint8_t>(currentLevel)) {
    return;
  }

  // Format: [LEVEL][TAG] message
  Serial.printf_P(PSTR("[%s][%s] "), levelToString(level), tag);

  // Handle variadic arguments
  va_list args;
  va_start(args, fmt);
  
  // Use a stack buffer for formatting
  char buffer[128];
  // vsnprintf_P expects a PGM pointer for format
  vsnprintf_P(buffer, sizeof(buffer), (const char*)fmt, args);
  
  va_end(args);
  
  Serial.println(buffer);
}
