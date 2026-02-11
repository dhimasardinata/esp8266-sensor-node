/**
 * @file Logger.h
 * @brief Lightweight logging abstraction with runtime-configurable log levels.
 * 
 * Log levels are stored in AppConfig and can be changed without recompilation.
 * Usage:
 *   LOG_DEBUG("REDACTED", "REDACTED", ssid);
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
   * @param tag Component/module identifier (e.g., "REDACTED", "REDACTED")
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
// Gate evaluation of arguments to avoid formatting work when level is disabled.

#define LOG_DEBUG(tag, ...)                                                                                        \
  do {                                                                                                             \
    if (static_cast<uint8_t>(LogLevel::DEBUG) >= static_cast<uint8_t>(Logger::currentLevel))                       \
      Logger::log(LogLevel::DEBUG, tag, __VA_ARGS__);                                                              \
  } while (0)

#define LOG_INFO(tag, ...)                                                                                         \
  do {                                                                                                             \
    if (static_cast<uint8_t>(LogLevel::INFO) >= static_cast<uint8_t>(Logger::currentLevel))                        \
      Logger::log(LogLevel::INFO, tag, __VA_ARGS__);                                                               \
  } while (0)

#define LOG_WARN(tag, ...)                                                                                         \
  do {                                                                                                             \
    if (static_cast<uint8_t>(LogLevel::WARN) >= static_cast<uint8_t>(Logger::currentLevel))                        \
      Logger::log(LogLevel::WARN, tag, __VA_ARGS__);                                                               \
  } while (0)

#define LOG_ERROR(tag, ...)                                                                                        \
  do {                                                                                                             \
    if (static_cast<uint8_t>(LogLevel::ERROR) >= static_cast<uint8_t>(Logger::currentLevel))                       \
      Logger::log(LogLevel::ERROR, tag, __VA_ARGS__);                                                              \
  } while (0)

#endif  // LOGGER_H
