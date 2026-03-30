#ifndef TEXT_BUFFER_UTILS_H
#define TEXT_BUFFER_UTILS_H

#include <Arduino.h>

#include <cstdint>
#include <cstring>

namespace TextBufferUtils {

  [[maybe_unused]] inline void copy_trunc(char* dst, size_t dst_len, const char* src, size_t src_len) {
    if (!dst || dst_len == 0) {
      return;
    }
    const size_t n = (src_len < (dst_len - 1)) ? src_len : (dst_len - 1);
    if (n > 0) {
      memcpy(dst, src, n);
    }
    dst[n] = '\0';
  }

  [[maybe_unused]] inline void copy_trunc(char* dst, size_t dst_len, const char* src) {
    if (!dst || dst_len == 0) {
      return;
    }
    if (!src) {
      dst[0] = '\0';
      return;
    }
    const size_t n = strnlen(src, dst_len - 1);
    if (n > 0) {
      memcpy(dst, src, n);
    }
    dst[n] = '\0';
  }

  [[maybe_unused]] inline void copy_trunc_P(char* dst, size_t dst_len, PGM_P src) {
    if (!dst || dst_len == 0) {
      return;
    }
    if (!src) {
      dst[0] = '\0';
      return;
    }
    size_t n = strlen_P(src);
    if (n > dst_len - 1) {
      n = dst_len - 1;
    }
    if (n > 0) {
      memcpy_P(dst, src, n);
    }
    dst[n] = '\0';
  }

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
    const size_t remaining = out_len - pos;
    if (remaining <= 1) {
      return pos;
    }
    const size_t n = strnlen(text, remaining - 1);
    memcpy(out + pos, text, n);
    pos += n;
    out[pos] = '\0';
    return pos;
  }

  [[maybe_unused]] inline size_t append_literal_P(char* out, size_t out_len, size_t pos, PGM_P text) {
    if (!out || pos >= out_len || !text) {
      return pos;
    }
    const size_t remaining = out_len - pos;
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

  [[maybe_unused]] inline size_t append_u32(char* out, size_t out_len, size_t pos, uint32_t value) {
    if (!out || pos >= out_len) {
      return pos;
    }
    const size_t remaining = out_len - pos;
    if (remaining <= 1) {
      return pos;
    }
    const size_t n = u32_to_dec(out + pos, remaining, value);
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

  [[maybe_unused]] inline bool append_bytes_strict(char* out,
                                                   size_t out_len,
                                                   size_t& pos,
                                                   const char* text,
                                                   size_t text_len) {
    if (!out || !text) {
      return false;
    }
    if (pos + text_len >= out_len) {
      return false;
    }
    if (text_len > 0) {
      memcpy(out + pos, text, text_len);
      pos += text_len;
      out[pos] = '\0';
    }
    return true;
  }

  [[maybe_unused]] inline bool append_bytes_strict_P(char* out, size_t out_len, size_t& pos, PGM_P text) {
    if (!out || !text) {
      return false;
    }
    const size_t text_len = strlen_P(text);
    if (pos + text_len >= out_len) {
      return false;
    }
    if (text_len > 0) {
      memcpy_P(out + pos, text, text_len);
      pos += text_len;
      out[pos] = '\0';
    }
    return true;
  }

  [[maybe_unused]] inline bool append_char_strict(char* out, size_t out_len, size_t& pos, char c) {
    if (!out || pos + 1 >= out_len) {
      return false;
    }
    out[pos++] = c;
    out[pos] = '\0';
    return true;
  }

  [[maybe_unused]] inline bool append_u32_strict(char* out, size_t out_len, size_t& pos, uint32_t value) {
    char tmp[10];
    const size_t n = u32_to_dec(tmp, sizeof(tmp), value);
    return append_bytes_strict(out, out_len, pos, tmp, n);
  }

  [[maybe_unused]] inline bool append_i32_strict(char* out, size_t out_len, size_t& pos, int32_t value) {
    uint32_t uval;
    if (value < 0) {
      if (!append_char_strict(out, out_len, pos, '-')) {
        return false;
      }
      uval = static_cast<uint32_t>(-(static_cast<int64_t>(value)));
    } else {
      uval = static_cast<uint32_t>(value);
    }
    return append_u32_strict(out, out_len, pos, uval);
  }

  [[maybe_unused]] inline bool append_fixed1_strict(char* out, size_t out_len, size_t& pos, int32_t value10) {
    if (value10 < 0) {
      if (!append_char_strict(out, out_len, pos, '-')) {
        return false;
      }
      value10 = -value10;
    }
    const uint32_t int_part = static_cast<uint32_t>(value10 / 10);
    const uint32_t frac = static_cast<uint32_t>(value10 % 10);
    if (!append_u32_strict(out, out_len, pos, int_part)) {
      return false;
    }
    if (!append_char_strict(out, out_len, pos, '.')) {
      return false;
    }
    return append_char_strict(out, out_len, pos, static_cast<char>('0' + frac));
  }

}  // namespace TextBufferUtils

#endif  // TEXT_BUFFER_UTILS_H
