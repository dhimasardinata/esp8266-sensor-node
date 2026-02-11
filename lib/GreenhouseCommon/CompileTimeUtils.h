#ifndef COMPILE_TIME_UTILS_H
#define COMPILE_TIME_UTILS_H

#include <algorithm>  // Required for std::copy_n
#include <array>
#include <string_view>

namespace CompileTimeUtils {

  // A string container that can be fully constructed and manipulated at compile time.
  // N: The size of the character array, including the null terminator.
  template <size_t N>
  struct FixedString {
    std::array<char, N> data{};

    // Constructor from a C-style char array reference.
    // Enables Class Template Argument Deduction (CTAD) from string literals.
    // Example: FixedString("abc") will correctly deduce N=4.
    consteval FixedString(const char (&str)[N]) {
      std::copy_n(str, N, data.begin());
    }

    // Constructor from a std::array.
    // Used by helper functions like concat() that build the string in an array.
    consteval FixedString(const std::array<char, N>& arr) : data(arr) {}

    consteval size_t size() const {
      return N - 1;
    }
    consteval const char* c_str() const {
      return data.data();
    }
  };

  // Concatenates multiple FixedString objects at compile time.
  template <FixedString S1, FixedString S2, FixedString... Rest>
  consteval auto concat() {
    if constexpr (sizeof...(Rest) == 0) {
      constexpr size_t N = S1.size() + S2.size() + 1;
      std::array<char, N> data{};
      char* out = data.data();
      for (size_t i = 0; i < S1.size(); ++i) {
        *out++ = S1.data[i];
      }
      for (size_t i = 0; i < S2.size(); ++i) {
        *out++ = S2.data[i];
      }
      *out = '\0';
      return FixedString(data);  // Calls the std::array constructor
    } else {
      // Recursively concatenate the first two strings, then the rest
      return concat<concat<S1, S2>(), Rest...>();
    }
  }

  // Counts the number of decimal digits in an integer at compile time.
  consteval size_t count_digits(unsigned int n) {
    if (n == 0)
      return 1;
    size_t count = 0;
    unsigned int temp = n;
    while (temp > 0) {
      temp /= 10;
      count++;
    }
    return count;
  }

  // Converts an integer to a FixedString at compile time.
  // V: The unsigned integer value to convert.
  template <unsigned int V>
  consteval auto to_fixed_string() {
    constexpr size_t NumDigits = count_digits(V);
    constexpr size_t BufferSize = NumDigits + 1;  // +1 for null terminator

    std::array<char, BufferSize> buffer{};
    char* end = buffer.data() + NumDigits;
    *end = '\0';  // Null terminate
    char* it = end;

    unsigned int temp = V;

    // This loop works correctly even for V = 0
    do {
      *--it = '0' + (temp % 10);
      temp /= 10;
    } while (temp > 0);

    return FixedString(buffer);  // Construct FixedString from the filled char array
  }

  // ========================================================================
  // Additional Compile-Time Utilities
  // ========================================================================

  // Compile-time string length (safer than strlen, zero runtime cost).
  template <size_t N>
  consteval size_t ct_strlen(const char (&)[N]) {
    return N - 1;
  }

  // Compile-time array size (type-safe sizeof alternative).
  template <typename T, size_t N>
  consteval size_t ct_array_size(const T (&)[N]) {
    return N;
  }

  // FNV-1a hash at compile time for string comparison.
  // Use this to compare strings without runtime strcmp overhead.
  consteval uint32_t ct_hash(const char* str, size_t len) {
    uint32_t hash = 2166136261u;  // FNV offset basis
    for (size_t i = 0; i < len; ++i) {
      hash ^= static_cast<uint8_t>(str[i]);
      hash *= 16777619u;  // FNV prime
    }
    return hash;
  }

  // FNV-1a hash from string literal.
  template <size_t N>
  consteval uint32_t ct_hash(const char (&str)[N]) {
    return ct_hash(str, N - 1);
  }

  // Runtime FNV-1a hash for user input comparison.
  // Same algorithm as ct_hash for compatibility.
  inline uint32_t rt_hash(const char* str) {
    uint32_t hash = 2166136261u;
    while (*str) {
      hash ^= static_cast<uint8_t>(*str++);
      hash *= 16777619u;
    }
    return hash;
  }

  // Compile-time min/max.
  template <typename T>
  consteval T ct_min(T a, T b) { return (a < b) ? a : b; }

  template <typename T>
  consteval T ct_max(T a, T b) { return (a > b) ? a : b; }

  // Compile-time clamp.
  template <typename T>
  consteval T ct_clamp(T val, T lo, T hi) {
    return ct_min(ct_max(val, lo), hi);
  }

  // Compile-time power of 2 check.
  consteval bool is_power_of_2(size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
  }

  // Compile-time next power of 2.
  consteval size_t next_power_of_2(size_t n) {
    if (n == 0) return 1;
    --n;
    n |= n >> 1; n |= n >> 2; n |= n >> 4;
    n |= n >> 8; n |= n >> 16;
    return ++n;
  }

  // ========================================================================
  // Additional Compile-Time Utilities for Zero-Cost Abstractions
  // ========================================================================

  // Compile-time absolute value.
  template <typename T>
  consteval T ct_abs(T val) {
    return val < 0 ? -val : val;
  }

  // Compile-time integer logarithm base 2 (floor).
  consteval size_t ct_log2(size_t n) {
    size_t result = 0;
    while (n >>= 1) ++result;
    return result;
  }

  // Compile-time power (base^exp).
  consteval size_t ct_pow(size_t base, size_t exp) {
    size_t result = 1;
    for (size_t i = 0; i < exp; ++i) result *= base;
    return result;
  }

  // Compile-time modulo for positive integers.
  consteval size_t ct_mod(size_t a, size_t b) {
    return a - (a / b) * b;
  }

  // Compile-time string literal length (guaranteed zero runtime cost).
  // Usage: constexpr auto len = ct_strlen_literal("hello"); // len = 5
  template <size_t N>
  consteval size_t ct_strlen_literal(const char (&)[N]) {
    return N - 1;
  }

  // Compile-time string equality check.
  consteval bool ct_streq(const char* a, const char* b, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      if (a[i] != b[i]) return false;
    }
    return true;
  }

  // Compile-time byte swap (endianness conversion).
  consteval uint16_t ct_bswap16(uint16_t val) {
    return static_cast<uint16_t>((val >> 8) | (val << 8));
  }

  consteval uint32_t ct_bswap32(uint32_t val) {
    return ((val & 0xFF000000u) >> 24) |
           ((val & 0x00FF0000u) >> 8) |
           ((val & 0x0000FF00u) << 8) |
           ((val & 0x000000FFu) << 24);
  }

  // Compile-time buffer size validation.
  // Use with static_assert to catch buffer overflows at compile time.
  template <size_t BufSize, size_t RequiredSize>
  consteval bool ct_fits_in_buffer() {
    return RequiredSize <= BufSize;
  }

  // Compile-time JSON key-value pair size estimation.
  // Useful for pre-calculating buffer sizes.
  template <size_t KeyLen, size_t MaxValueLen>
  consteval size_t ct_json_pair_size() {
    // "key":value, = key + 4 (quotes + colon + comma) + value
    return KeyLen + 4 + MaxValueLen;
  }

  // Compile-time JSON object size estimation.
  // NumPairs: Number of key-value pairs.
  // AvgPairSize: Average size per pair.
  template <size_t NumPairs, size_t AvgPairSize>
  consteval size_t ct_json_object_size() {
    // {} + pairs + some margin for nesting
    return 2 + (NumPairs * AvgPairSize) + 8;
  }

  // ========================================================================
  // Compile-Time String to std::array Helper
  // ========================================================================

  // Convert a string literal to std::array<char, N> at compile time.
  // Required for constexpr struct initialization with std::array fields.
  // Usage: constexpr auto arr = ct_string_to_array<45>("my string");
  template <size_t N>
  consteval std::array<char, N> ct_string_to_array(const char* str) {
    std::array<char, N> result{};
    size_t i = 0;
    while (i < N - 1 && str[i] != '\0') {
      result[i] = str[i];
      ++i;
    }
    // Rest is already zero-initialized
    return result;
  }

  // Overload for string literal deduction
  template <size_t N, size_t SrcLen>
  consteval std::array<char, N> ct_make_array(const char (&str)[SrcLen]) {
    static_assert(SrcLen <= N, "String literal too long for target array");
    std::array<char, N> result{};
    for (size_t i = 0; i < SrcLen && i < N; ++i) {
      result[i] = str[i];
    }
    return result;
  }

}  // namespace CompileTimeUtils

#endif  // COMPILE_TIME_UTILS_H