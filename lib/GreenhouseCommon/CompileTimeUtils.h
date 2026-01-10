#ifndef COMPILE_TIME_UTILS_H
#define COMPILE_TIME_UTILS_H

#include <algorithm>  // Required for std::copy_n
#include <array>
#include <string_view>

namespace CompileTimeUtils {

  /**
   * @brief A string container that can be fully constructed and manipulated at compile time.
   * @tparam N The size of the character array, including the null terminator.
   */
  template <size_t N>
  struct FixedString {
    std::array<char, N> data{};

    /**
     * @brief Constructor from a C-style char array reference.
     * @details This enables Class Template Argument Deduction (CTAD) from string literals.
     *          For example, FixedString("abc") will correctly deduce N=4.
     */
    consteval FixedString(const char (&str)[N]) {
      std::copy_n(str, N, data.begin());
    }

    /**
     * @brief Constructor from a std::array.
     * @details This is used by helper functions like concat() that build the string in an array.
     */
    consteval FixedString(const std::array<char, N>& arr) : data(arr) {}

    consteval size_t size() const {
      return N - 1;
    }
    consteval const char* c_str() const {
      return data.data();
    }
  };

  /**
   * @brief Concatenates multiple FixedString objects at compile time.
   */
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

  /**
   * @brief Counts the number of decimal digits in an integer at compile time.
   */
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

  /**
   * @brief Converts an integer to a FixedString at compile time.
   * @tparam V The unsigned integer value to convert.
   */
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

}  // namespace CompileTimeUtils

#endif  // COMPILE_TIME_UTILS_H