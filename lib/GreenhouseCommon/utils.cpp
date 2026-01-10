#include "utils.h"

#include <ESP8266WiFi.h>
#include <bearssl/bearssl_hash.h>
#include <time.h>

#include <new>
#include <vector>

#include "Logger.h"

namespace Utils {

  void copy_string(std::span<char> dest, std::string_view src) {
    if (dest.empty())
      return;
    size_t to_copy = std::min(src.length(), dest.size() - 1);
    memcpy(dest.data(), src.data(), to_copy);
    dest[to_copy] = '\0';
  }

  void trim_inplace(std::span<char> s) {
    if (s.empty() || s[0] == '\0')
      return;
    char* start = s.data();
    char* p = start;
    while (*p && isspace((unsigned char)*p))
      ++p;

    size_t leading_spaces = p - start;
    size_t content_len = strnlen(p, s.size() - leading_spaces);
    memmove(start, p, content_len + 1);

    size_t new_len = content_len;
    while (new_len > 0 && isspace((unsigned char)start[new_len - 1])) {
      start[--new_len] = '\0';
    }
  }

  size_t hash_sha256(std::span<char> output_hex, std::string_view input) {
    if (output_hex.size() < 65)
      return 0;
    uint8_t hash_binary[32];
    br_sha256_context ctx;
    br_sha256_init(&ctx);
    br_sha256_update(&ctx, input.data(), input.length());
    br_sha256_out(&ctx, hash_binary);

    char* out_ptr = output_hex.data();
    for (int i = 0; i < 32; i++) {
      snprintf(out_ptr + (i * 2), 3, "%02x", hash_binary[i]);
    }
    out_ptr[64] = '\0';
    return 64;
  }

  // Destructive in-place parser (Zero Allocation)
  size_t tokenize_quoted_args(char* input, const char* argv[], size_t max_args) {
    size_t count = 0;
    char* p = input;

    while (*p && count < max_args) {
      // 1. Skip leading spaces
      while (*p == ' ')
        p++;
      if (*p == '\0')
        break;  // End of string

      // 2. Is this a quoted argument?
      if (*p == '"') {
        p++;                // Move past the opening quote
        argv[count++] = p;  // Point arg to start of content

        // Find the closing quote
        char* end = strchr(p, '"');
        if (end) {
          *end = '\0';  // Replace closing quote with NULL
          p = end + 1;  // Continue parsing after the quote
        } else {
          // No closing quote found? Take rest of string
          // (This handles error case gracefully)
          break;
        }
      }
      // 3. It is a regular argument
      else {
        argv[count++] = p;  // Point arg to start

        // Find the next space
        char* end = strchr(p, ' ');
        if (end) {
          *end = '\0';  // Replace space with NULL
          p = end + 1;  // Continue parsing
        } else {
          // End of string reached
          break;
        }
      }
    }

    return count;
  }

  void scramble_data(std::span<char> data) {
    if (data.empty())
      return;
    uint32_t chipId = ESP.getChipId();
    const uint8_t* key = (const uint8_t*)&chipId;

    // --- BUG FIX: TRUNCATION ---
    // Don't use strnlen. Ciphertext can contain valid 0x00 bytes
    // (especially if input char XOR key produces 0).
    // We must process the entire fixed-size buffer.
    size_t len = data.size();

    for (size_t i = 0; i < len; ++i) {
      data[i] = data[i] ^ key[i % 4] ^ 0x5A;
    }
  }

  // Implementation of Fix #4 - OPTIMIZED: Static cipher to avoid re-init overhead
  void ws_send_encrypted(AsyncWebSocketClient* client, const char* plainText) {
    if (!client)
      return;

    // OPTIMIZATION: Static cipher instance (reused across calls)
    // Thread-safety note: ESP8266 is single-threaded, so this is safe
    static CryptoUtils::AES_CBC_Cipher cipher(std::string_view(reinterpret_cast<const char*>(CryptoUtils::AES_KEY), 32));

    auto encrypted_payload = cipher.encrypt(plainText);
    if (encrypted_payload) {
      String serialized = CryptoUtils::serialize_payload(*encrypted_payload);
      client->text(serialized);
    }
  }

  // OPTIMIZED: Use static buffer for small messages to reduce heap fragmentation
  void ws_printf(AsyncWebSocketClient* client, const char* fmt, ...) {
    if (!client || !client->canSend())
      return;

    va_list args;

    // STEP 1: Calculate the length of the string to be produced
    va_start(args, fmt);
    int len = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    if (len < 0)
      return;  // Encoding error

    // OPTIMIZATION: Use static buffer for small messages (most common case)
    constexpr size_t STATIC_BUF_SIZE = 256;
    static char staticBuf[STATIC_BUF_SIZE];
    char* buf = nullptr;
    std::unique_ptr<char[]> heapBuf;

    if (static_cast<size_t>(len + 1) <= STATIC_BUF_SIZE) {
      // Small message: use stack/static buffer (no heap allocation)
      buf = staticBuf;
    } else {
      // Large message: fall back to heap allocation
      heapBuf.reset(new (std::nothrow) char[len + 1]);
      if (!heapBuf) {
        LOG_ERROR("UTILS", F("OOM in ws_printf"));
        return;
      }
      buf = heapBuf.get();
    }

    // STEP 2: Write string to buffer
    va_start(args, fmt);
    vsnprintf(buf, len + 1, fmt, args);
    va_end(args);

    // Send (this function will perform encryption)
    ws_send_encrypted(client, buf);
  }

  // Helper to convert Month string to index (0-11)
  static int get_month_index(const char* m) {
    // Data-driven lookup table for cleaner, more maintainable code
    static constexpr struct { const char* name; int index; } months[] = {
        {"Jan", 0}, {"Feb", 1}, {"Mar", 2}, {"Apr", 3},
        {"May", 4}, {"Jun", 5}, {"Jul", 6}, {"Aug", 7},
        {"Sep", 8}, {"Oct", 9}, {"Nov", 10}, {"Dec", 11}
    };
    for (const auto& month : months) {
      if (strncmp(m, month.name, 3) == 0) return month.index;
    }
    return 0;  // Default to January if not found
  }

  time_t parse_http_date(const char* dateStr) {
    if (!dateStr || strlen(dateStr) < 10)
      return 0;

    // Expected format: "Wed, 21 Oct 2015 07:28:00 GMT"
    int day, year, h, m, s;
    char monthStr[4];
    char weekday[4];

    // %3s reads 3 chars for weekday, %d for day, %3s for month, etc.
    // Note: The format string handles the commas and spaces.
    int parsed = sscanf(dateStr, "%3s, %d %3s %d %d:%d:%d", weekday, &day, monthStr, &year, &h, &m, &s);

    if (parsed != 7)
      return 0;

    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = get_month_index(monthStr);
    t.tm_mday = day;
    t.tm_hour = h;
    t.tm_min = m;
    t.tm_sec = s;
    t.tm_isdst = 0;  // HTTP Date is always GMT

    // FIX: mktime() interprets struct tm as LOCAL time, but HTTP Date is GMT.
    // We use a portable approach: call mktime() then subtract the timezone offset.
    time_t local_result = mktime(&t);
    if (local_result == (time_t)-1)
      return 0;
    
    // Compensate for local timezone - mktime assumed local, but input was GMT
    // We add the offset because mktime "thought" GMT time was local time,
    // so it produced a value that's (offset) seconds too early.
    return local_result + _timezone;  // _timezone is seconds WEST of UTC (negated on ESP8266)
  }

  bool isSafeString(std::string_view str) {
      if (str.empty()) return true; // Empty is considered safe structure-wise
      for (char c : str) {
          // Allow printable ASCII (32-126)
          if (c < 32 || c > 126) {
              return false;
          }
      }
      return true;
  }
}  // namespace Utils