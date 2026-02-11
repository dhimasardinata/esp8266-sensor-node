#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

#include <memory>
#include <span>
#include <string_view>

#include "CryptoUtils.h"

class AsyncWebSocketClient;

namespace Utils {

  void copy_string(std::span<char> dest, std::string_view src);
  void trim_inplace(std::span<char> s);
  [[nodiscard]] size_t hash_sha256(std::span<char> output_hex, std::string_view input);
  [[nodiscard]] size_t tokenize_quoted_args(char* input, const char* argv[], size_t max_args);
  void scramble_data(std::span<char> data);
  const char* redact(const char* input, std::span<char> out, size_t keep_head = 2, size_t keep_tail = 1);
  [[nodiscard]] bool consttime_equal(const char* a, const char* b, size_t len);

  // Helper enkripsi
  void ws_send_encrypted(AsyncWebSocketClient* client, std::string_view plainText);
  void ws_send_encrypted(AsyncWebSocketClient* client, const char* plainText);
  // Enable/disable WS buffers to free heap in portal mode.
  [[nodiscard]] bool ws_set_enabled(bool enabled);

  // --- SMART PRINTF (Menghitung ukuran otomatis) ---
  // Tidak perlu template <N> lagi.
  __attribute__((format(printf, 2, 3))) void ws_printf(AsyncWebSocketClient* client, const char* fmt, ...);

  // --- NEW: HTTP Date Parser ---
  /**
   * @brief Parses HTTP Date header format to Unix timestamp.
   * @param dateStr Date string like "Wed, 21 Oct 2015 07:28:00 GMT"
   * @return Unix timestamp, or 0 on parse failure.
   */
  [[nodiscard]] time_t parse_http_date(const char* dateStr);

  /**
   * @brief Validates that a string contains only safe printable ASCII chars (32-126).
   * @param str Input string to validate.
   * @return true if all characters are safe, false if any control or non-ASCII chars present.
   */
  [[nodiscard]] bool isSafeString(std::string_view str);

  /**
   * @brief Escapes special JSON characters in a string to prevent injection.
   * Uses stack buffer to avoid heap fragmentation.
   * @param dest Destination buffer (should be 2x input size + 1 for safety)
   * @param src Source string to escape
   * @return Number of characters written (excluding null terminator)
   */
  [[nodiscard]] size_t escape_json_string(std::span<char> dest, std::string_view src);

  class InterruptGuard {
  public:
    InterruptGuard() { noInterrupts(); }
    ~InterruptGuard() { interrupts(); }
    InterruptGuard(const InterruptGuard&) = delete;
    InterruptGuard& operator=(const InterruptGuard&) = delete;
  };

}  // namespace Utils

#endif
