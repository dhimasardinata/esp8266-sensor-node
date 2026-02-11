#include "utils.h"

#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <bearssl/bearssl_hash.h>
#include <time.h>

#include <array>
#include <memory>
#include <new>

#include "Logger.h"

namespace Utils {

  void copy_string(std::span<char> dest, std::string_view src) {
    if (dest.empty())
      return;
    memset(dest.data(), 0, dest.size());
    size_t len = std::min(dest.size() - 1, src.size());
    if (len > 0) {
      memcpy(dest.data(), src.data(), len);
    }
    dest[len] = '\0';
  }

  void trim_inplace(std::span<char> s) {
    if (s.empty() || s[0] == '\0')
      return;
    char* start = s.data();
    char* p = start;
    while (*p && isspace((unsigned char)*p))
      ++p;
    size_t leading = p - start;
    size_t len = strnlen(p, s.size() - leading);
    if (leading > 0 && len > 0) {
      memmove(start, p, len + 1);
    } else if (leading > 0) {
      // String is all whitespace.
      start[0] = '\0';
      return;
    }
    while (len > 0 && isspace((unsigned char)start[len - 1]))
      start[--len] = '\0';
  }

  size_t hash_sha256(std::span<char> output_hex, std::string_view input) {
    if (output_hex.size() < 65)
      return 0;
    uint8_t hash_binary[32];
    br_sha256_context ctx;
    br_sha256_init(&ctx);
    br_sha256_update(&ctx, input.data(), input.length());
    br_sha256_out(&ctx, hash_binary);

    static constexpr char kHex[] = "0123456789abcdef";
    char* out_ptr = output_hex.data();
    for (int i = 0; i < 32; i++) {
      out_ptr[i * 2] = kHex[(hash_binary[i] >> 4) & 0x0F];
      out_ptr[i * 2 + 1] = kHex[hash_binary[i] & 0x0F];
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

    // Safety: Prevent truncation.
    // Don't use strnlen. Ciphertext can contain valid 0x00 bytes.
    // We must process the entire fixed-size buffer.
    size_t len = data.size();

#ifdef ENABLE_STRONG_SCRAMBLE
    // Stronger stream mask using SHA256(chipId || blockIdx || AES_KEY).
    for (size_t offset = 0; offset < len; offset += 32) {
      br_sha256_context ctx;
      br_sha256_init(&ctx);
      br_sha256_update(&ctx, &chipId, sizeof(chipId));
      uint32_t blockIdx = static_cast<uint32_t>(offset / 32);
      br_sha256_update(&ctx, &blockIdx, sizeof(blockIdx));
      br_sha256_update(&ctx, CryptoUtils::AES_KEY, sizeof(CryptoUtils::AES_KEY));
      uint8_t digest[32];
      br_sha256_out(&ctx, digest);
      size_t chunk = (len - offset < 32) ? (len - offset) : 32;
      for (size_t i = 0; i < chunk; ++i) {
        data[offset + i] ^= static_cast<char>(digest[i]);
      }
    }
#else
    const uint8_t* key = (const uint8_t*)&chipId;
    for (size_t i = 0; i < len; ++i) {
      data[i] = data[i] ^ key[i % 4] ^ 0x5A;
    }
#endif
  }

  const char* redact(const char* input, std::span<char> out, size_t keep_head, size_t keep_tail) {
    if (out.empty())
      return "";
    if (!input) {
      const char* null_str = "<null>";
      copy_string(out, null_str);
      return out.data();
    }

    size_t in_len = strnlen(input, out.size() - 1);
    if (in_len == 0) {
      out[0] = '\0';
      return out.data();
    }

    // If too short, mask entire string.
    if (in_len <= (keep_head + keep_tail + 1)) {
      size_t n = std::min(in_len, out.size() - 1);
      for (size_t i = 0; i < n; ++i) {
        out[i] = '*';
      }
      out[n] = '\0';
      return out.data();
    }

    size_t write_len = std::min(in_len, out.size() - 1);
    size_t head = std::min(keep_head, write_len);
    size_t tail = std::min(keep_tail, write_len - head);
    size_t mask_len = write_len - head - tail;

    memcpy(out.data(), input, head);
    memset(out.data() + head, '*', mask_len);
    memcpy(out.data() + head + mask_len, input + (in_len - tail), tail);
    out[write_len] = '\0';
    return out.data();
  }

  bool consttime_equal(const char* a, const char* b, size_t len) {
    if (!a || !b)
      return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < len; ++i) {
      diff |= static_cast<uint8_t>(a[i]) ^ static_cast<uint8_t>(b[i]);
    }
    return diff == 0;
  }

  namespace {
    constexpr size_t WS_QUEUE_SIZE = 2;  // power of two
    static_assert((WS_QUEUE_SIZE & (WS_QUEUE_SIZE - 1)) == 0, "WS_QUEUE_SIZE must be power of two");
    struct WsQueuedChunk {
      std::array<char, CryptoUtils::MAX_PLAINTEXT_SIZE> data{};
      size_t len = 0;
    };
    struct WsState {
      std::array<char, CryptoUtils::ENCRYPTION_BUFFER_SIZE> buf{};
      bool busy = false;
      std::array<WsQueuedChunk, WS_QUEUE_SIZE> queue{};
      size_t qHead = 0;
      size_t qTail = 0;
      uint32_t dropCount = 0;
      std::unique_ptr<char[]> printfBuf;
      size_t printfCap = 0;
      bool printfBusy = false;
    };

    std::unique_ptr<WsState> g_wsState;
    bool g_wsEnabled = false;
    void ws_try_free() {
      if (g_wsEnabled || !g_wsState) {
        return;
      }
      if (g_wsState->busy || g_wsState->printfBusy) {
        return;
      }
      g_wsState.reset();
      CryptoUtils::releaseWsCipher();
    }
  }

  bool ws_set_enabled(bool enabled) {
    if (enabled) {
      // Lazy allocation: buffers are created on first use.
      g_wsEnabled = true;
      return true;
    }

    g_wsEnabled = false;
    ws_try_free();
    return true;
  }

  WsState* ensure_ws_state() {
    if (!g_wsEnabled) {
      return nullptr;
    }
    if (g_wsState) {
      return g_wsState.get();
    }
    std::unique_ptr<WsState> state(new (std::nothrow) WsState());
    if (!state) {
      return nullptr;
    }
    g_wsState.swap(state);
    return g_wsState.get();
  }

  void ws_send_encrypted(AsyncWebSocketClient* client, std::string_view plainText) {
    WsState* state = ensure_ws_state();
    if (!state)
      return;

    if (!client || !client->canSend())
      return;

    if (plainText.empty())
      return;

    auto enqueue_chunks = [&](std::string_view text) {
      const size_t maxChunk = CryptoUtils::MAX_PLAINTEXT_SIZE;
      size_t offset = 0;
      bool overflowed = false;
      while (offset < text.size()) {
        size_t chunk_len = text.size() - offset;
        if (chunk_len > maxChunk)
          chunk_len = maxChunk;
        bool enqueued = false;
        {
          InterruptGuard guard;
          size_t next = (state->qHead + 1) & (WS_QUEUE_SIZE - 1);
          if (next != state->qTail) {
            auto& slot = state->queue[state->qHead];
            if (chunk_len > slot.data.size())
              chunk_len = slot.data.size();
            if (chunk_len > 0) {
              memcpy(slot.data.data(), text.data() + offset, chunk_len);
            }
            slot.len = chunk_len;
            state->qHead = next;
            enqueued = true;
          }
        }
        if (!enqueued) {
          overflowed = true;
          break;
        }
        offset += chunk_len;
      }
      if (overflowed) {
        ++state->dropCount;
        LOG_WARN("WS", F("WS queue overflow (%u drops)"), state->dropCount);
      }
    };

    {
      InterruptGuard guard;
      if (state->busy) {
        enqueue_chunks(plainText);
        return;
      }
      state->busy = true;
    }

    auto send_chunks = [&](const char* data, size_t len) {
      const size_t maxChunk = CryptoUtils::MAX_PLAINTEXT_SIZE;
      size_t offset = 0;
      while (offset < len) {
        size_t chunk_len = len - offset;
        if (chunk_len > maxChunk)
          chunk_len = maxChunk;
        size_t written = CryptoUtils::fast_serialize_encrypted_ws(
            std::string_view(data + offset, chunk_len), state->buf.data(), state->buf.size());
        if (written == 0) {
          break;
        }
        client->text(state->buf.data(), written);
        offset += chunk_len;
      }
    };

    send_chunks(plainText.data(), plainText.size());

    // Drain queued chunks without risking overwrite by enqueuers.
    std::array<char, CryptoUtils::MAX_PLAINTEXT_SIZE> wsPlainBuf{};
    while (true) {
      size_t local_len = 0;
      {
        InterruptGuard guard;
        if (state->qHead == state->qTail)
          break;
        auto& slot = state->queue[state->qTail];
        local_len = slot.len;
        if (local_len > 0) {
          memcpy(wsPlainBuf.data(), slot.data.data(), local_len);
        }
        state->qTail = (state->qTail + 1) & (WS_QUEUE_SIZE - 1);
      }
      if (local_len > 0) {
        send_chunks(wsPlainBuf.data(), local_len);
      }
    }

    {
      InterruptGuard guard;
      state->busy = false;
    }
    if (!g_wsEnabled) {
      ws_try_free();
    }
  }

  void ws_send_encrypted(AsyncWebSocketClient* client, const char* plainText) {
    if (!plainText)
      return;
    size_t len = strlen(plainText);
    ws_send_encrypted(client, std::string_view(plainText, len));
  }

  // Optimized: Reuse a shared heap buffer for long messages to avoid truncation and reduce fragmentation.
  void ws_printf(AsyncWebSocketClient* client, const char* fmt, ...) {
    WsState* state = ensure_ws_state();
    if (!state)
      return;

    if (!client || !client->canSend())
      return;

    va_list args;

    constexpr size_t STACK_BUF_SIZE = 256;
    char stackBuf[STACK_BUF_SIZE];

    va_start(args, fmt);
    int len = vsnprintf(stackBuf, sizeof(stackBuf), fmt, args);
    va_end(args);

    if (len < 0)
      return;  // Encoding error

    size_t needed = static_cast<size_t>(len);
    if (needed < sizeof(stackBuf)) {
      if (needed > 0) {
        ws_send_encrypted(client, std::string_view(stackBuf, needed));
      }
      return;
    }

    {
      InterruptGuard guard;
      if (state->printfBusy) {
        // Avoid re-entrancy; drop to prevent corruption.
        return;
      }
      state->printfBusy = true;
    }

    if (state->printfCap < needed + 1) {
      std::unique_ptr<char[]> newBuf(new (std::nothrow) char[needed + 1]);
      if (!newBuf) {
        LOG_ERROR("UTILS", F("OOM in ws_printf"));
        {
          InterruptGuard guard;
          state->printfBusy = false;
        }
        return;
      }
      state->printfBuf.swap(newBuf);
      state->printfCap = needed + 1;
    }

    va_start(args, fmt);
    vsnprintf(state->printfBuf.get(), state->printfCap, fmt, args);
    va_end(args);

    ws_send_encrypted(client, std::string_view(state->printfBuf.get(), needed));

    {
      InterruptGuard guard;
      state->printfBusy = false;
    }
    constexpr size_t kMaxPersistentPrintf = 256;
    if (state->printfCap > kMaxPersistentPrintf) {
      state->printfBuf.reset();
      state->printfCap = 0;
    }
    if (!g_wsEnabled) {
      ws_try_free();
    }
  }

  // Helper to convert Month string to index (0-11)
  static int get_month_index(const char* m) {
    // Data-driven lookup table for cleaner, more maintainable code
    static constexpr struct {
      const char* name;
      int index;
    } months[] = {{"Jan", 0},
                  {"Feb", 1},
                  {"Mar", 2},
                  {"Apr", 3},
                  {"May", 4},
                  {"Jun", 5},
                  {"Jul", 6},
                  {"Aug", 7},
                  {"Sep", 8},
                  {"Oct", 9},
                  {"Nov", 10},
                  {"Dec", 11}};
    for (const auto& month : months) {
      if (strncmp(m, month.name, 3) == 0)
        return month.index;
    }
    return 0;  // Default to January if not found
  }

  time_t parse_http_date(const char* dateStr) {
    // HTTP date format: "Wed, 21 Oct 2015 07:28:00 GMT" (29 chars)
    constexpr size_t HTTP_DATE_MAX_LEN = 30;
    constexpr size_t HTTP_DATE_MIN_LEN = 10;  // Minimum valid date length
    if (!dateStr || strnlen(dateStr, HTTP_DATE_MAX_LEN) < HTTP_DATE_MIN_LEN)
      return 0;

    // Expected format: "Wed, 21 Oct 2015 07:28:00 GMT"
    auto parse_2digits = [](const char* p, int& out) -> bool {
      if (!p || p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9')
        return false;
      out = (p[0] - '0') * 10 + (p[1] - '0');
      return true;
    };
    auto parse_4digits = [](const char* p, int& out) -> bool {
      if (!p)
        return false;
      out = 0;
      for (int i = 0; i < 4; ++i) {
        if (p[i] < '0' || p[i] > '9')
          return false;
        out = (out * 10) + (p[i] - '0');
      }
      return true;
    };

    const char* p = strchr(dateStr, ',');
    if (!p)
      return 0;
    ++p;
    while (*p == ' ')
      ++p;

    int day = 0;
    if (*p < '0' || *p > '9')
      return 0;
    day = (*p - '0');
    ++p;
    if (*p >= '0' && *p <= '9') {
      day = (day * 10) + (*p - '0');
      ++p;
    }
    while (*p == ' ')
      ++p;

    char monthStr[4] = {0};
    if (!p[0] || !p[1] || !p[2])
      return 0;
    monthStr[0] = p[0];
    monthStr[1] = p[1];
    monthStr[2] = p[2];
    p += 3;
    while (*p == ' ')
      ++p;

    int year = 0;
    if (!parse_4digits(p, year))
      return 0;
    p += 4;
    while (*p == ' ')
      ++p;

    int h = 0, m = 0, s = 0;
    if (!parse_2digits(p, h))
      return 0;
    p += 2;
    if (*p != ':')
      return 0;
    ++p;
    if (!parse_2digits(p, m))
      return 0;
    p += 2;
    if (*p != ':')
      return 0;
    ++p;
    if (!parse_2digits(p, s))
      return 0;

    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = get_month_index(monthStr);
    t.tm_mday = day;
    t.tm_hour = h;
    t.tm_min = m;
    t.tm_sec = s;
    t.tm_isdst = 0;  // HTTP Date is always GMT

    // Correction: mktime() interprets struct tm as LOCAL time, but HTTP Date is GMT.
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
    if (str.empty())
      return true;  // Empty is considered safe structure-wise
    for (char c : str) {
      // Allow printable ASCII (32-126)
      if (c < 32 || c > 126) {
        return false;
      }
    }
    return true;
  }

  size_t escape_json_string(std::span<char> dest, std::string_view src) {
    if (dest.empty())
      return 0;

    const size_t max_write = dest.size() - 1;  // Reserve space for null terminator
    const size_t src_len = src.size();
    if (src_len <= max_write) {
      bool needs_escape = false;
      for (char c : src) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 32 || uc > 126 || c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t') {
          needs_escape = true;
          break;
        }
      }
      if (!needs_escape) {
        if (src_len > 0) {
          memcpy(dest.data(), src.data(), src_len);
        }
        dest[src_len] = '\0';
        return src_len;
      }
    }

    size_t written = 0;

    for (char c : src) {
      if (written >= max_write)
        break;

      switch (c) {
        case '"':
          if (written + 2 > max_write)
            goto done;
          dest[written++] = '\\';
          dest[written++] = '"';
          break;
        case '\\':
          if (written + 2 > max_write)
            goto done;
          dest[written++] = '\\';
          dest[written++] = '\\';
          break;
        case '\n':
          if (written + 2 > max_write)
            goto done;
          dest[written++] = '\\';
          dest[written++] = 'n';
          break;
        case '\r':
          if (written + 2 > max_write)
            goto done;
          dest[written++] = '\\';
          dest[written++] = 'r';
          break;
        case '\t':
          if (written + 2 > max_write)
            goto done;
          dest[written++] = '\\';
          dest[written++] = 't';
          break;
        default:
          // Only include printable ASCII (32-126)
          if (c >= 32 && c < 127) {
            dest[written++] = c;
          }
          break;
      }
    }

  done:
    dest[written] = '\0';
    return written;
  }
}  // namespace Utils
