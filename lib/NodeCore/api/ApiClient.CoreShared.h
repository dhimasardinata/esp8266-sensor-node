#pragma once

// ApiClient.CoreShared.h - internal helper utilities shared across ApiClient core modules

namespace {
  [[maybe_unused]] inline size_t u32_to_dec(char* out, size_t out_len, uint32_t value) {
    if (!out || out_len == 0) {
      return 0;
    }
    char tmp[10];
    size_t n = 0;
    do {
      tmp[n++] = static_cast<char>('0' + (value % 10));
      value /= 10;
    } while (value != 0 && n < sizeof(tmp));
    size_t written = 0;
    while (n > 0 && written + 1 < out_len) {
      out[written++] = tmp[--n];
    }
    out[written] = '\0';
    return written;
  }

  [[maybe_unused]] inline size_t append_literal(char* out, size_t out_len, size_t pos, const char* text) {
    if (!out || pos >= out_len || !text) {
      return pos;
    }
    size_t remaining = out_len - pos;
    if (remaining <= 1) {
      return pos;
    }
    size_t n = strnlen(text, remaining - 1);
    memcpy(out + pos, text, n);
    pos += n;
    out[pos] = '\0';
    return pos;
  }

  [[maybe_unused]] inline size_t append_literal_P(char* out, size_t out_len, size_t pos, PGM_P text) {
    if (!out || pos >= out_len || !text) {
      return pos;
    }
    size_t remaining = out_len - pos;
    if (remaining <= 1) {
      return pos;
    }
    size_t n = strlen_P(text);
    if (n > remaining - 1) {
      n = remaining - 1;
    }
    memcpy_P(out + pos, text, n);
    pos += n;
    out[pos] = '\0';
    return pos;
  }

  [[maybe_unused]] inline void copy_trunc_P(char* out, size_t out_len, PGM_P text) {
    if (!out || out_len == 0) {
      return;
    }
    if (!text) {
      out[0] = '\0';
      return;
    }
    strncpy_P(out, text, out_len - 1);
    out[out_len - 1] = '\0';
  }

  [[maybe_unused]] inline size_t append_u32(char* out, size_t out_len, size_t pos, uint32_t value) {
    if (!out || pos >= out_len) {
      return pos;
    }
    size_t remaining = out_len - pos;
    if (remaining <= 1) {
      return pos;
    }
    size_t n = u32_to_dec(out + pos, remaining, value);
    pos += n;
    if (pos < out_len) {
      out[pos] = '\0';
    }
    return pos;
  }

  [[maybe_unused]] inline size_t append_i32(char* out, size_t out_len, size_t pos, int32_t value) {
    if (value < 0) {
      pos = append_literal_P(out, out_len, pos, PSTR("-"));
      value = -value;
    }
    return append_u32(out, out_len, pos, static_cast<uint32_t>(value));
  }

  [[maybe_unused]] inline size_t append_cstr(char* out, size_t out_len, size_t pos, const char* text) {
    return append_literal(out, out_len, pos, text);
  }
}  // namespace
