/**
 * @file Logger.h
 * @brief Lightweight logging abstraction with runtime-configurable log levels.
 * 
 * Log levels are stored in AppConfig and can be changed without recompilation.
 * Usage:
 *   LOG_DEBUG("WIFI", "Connecting to %s", ssid);
 *   LOG_INFO("API", "Upload complete");
 *   LOG_WARN("MEM", "Low heap: %u bytes", freeHeap);
 *   LOG_ERROR("SENSOR", "I2C read failed");
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

/**
 * @brief Log severity levels (lower = more verbose)
 */
enum class LogLevel : uint8_t {
  DEBUG = 0,  ///< Detailed debug information
  INFO = 1,   ///< General operational messages
  WARN = 2,   ///< Warning conditions
  ERROR = 3,  ///< Error conditions
  NONE = 255  ///< Disable all logging
};

/**
 * @brief Simple logging class with runtime-configurable level
 */
class Logger {
public:
  /// Current minimum log level (messages below this are suppressed)
  static LogLevel currentLevel;

  /**
   * @brief Set the minimum log level
   * @param level Messages with lower severity than this will be suppressed
   */
  static void setLevel(LogLevel level) noexcept;

  /**
   * @brief Log a message if its level meets the current threshold
   * @param level Severity of this message
   * @param tag Component/module identifier (e.g., "WIFI", "API")
   * @param fmt printf-style format string
   */
#ifdef NATIVE_TEST
  static void log(LogLevel level, const char* tag, const char* fmt, ...);
#else
  static void log(LogLevel level, const char* tag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
#endif
  static void log(LogLevel level, const char* tag, const __FlashStringHelper* fmt, ...);

  /**
   * @brief Get string representation of log level
   */
  [[nodiscard]] static const char* levelToString(LogLevel level) noexcept;
};

// --- Convenience Macros ---
// These check level at runtime, not compile-time

#define LOG_DEBUG(tag, ...) Logger::log(LogLevel::DEBUG, tag, __VA_ARGS__)
#define LOG_INFO(tag, ...)  Logger::log(LogLevel::INFO, tag, __VA_ARGS__)
#define LOG_WARN(tag, ...)  Logger::log(LogLevel::WARN, tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) Logger::log(LogLevel::ERROR, tag, __VA_ARGS__)

#endif  // LOGGER_H
