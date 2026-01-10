#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "CryptoUtils.h"

namespace Utils {

  void copy_string(std::span<char> dest, std::string_view src);
  void trim_inplace(std::span<char> s);
  [[nodiscard]] size_t hash_sha256(std::span<char> output_hex, std::string_view input);
  size_t tokenize_quoted_args(char* input, const char* argv[], size_t max_args);
  void scramble_data(std::span<char> data);

  // Helper enkripsi
  void ws_send_encrypted(AsyncWebSocketClient* client, const char* plainText);

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

}  // namespace Utils

#endif