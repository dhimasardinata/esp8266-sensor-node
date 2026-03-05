#ifndef COMPILE_TIME_JSON_H
#define COMPILE_TIME_JSON_H

/**
 * @file CompileTimeJSON.h
 * @brief Compile-time JSON string building for zero-allocation payloads.
 * 
 * This enables building static JSON templates at compile time, avoiding:
 * - Runtime string concatenation
 * - Heap allocation
 * - snprintf overhead for static parts
 * 
 * Usage:
 *   // Build a compile-time JSON template
 *   constexpr auto json = ct_json::object(
 *     ct_json::pair("type", ct_json::string("sensor")),
 *     ct_json::pair("node_id", ct_json::placeholder()),  // %d at runtime
 *     ct_json::pair("version", ct_json::string("1.0"))
 *   );
 *   
 *   // At runtime, use the template with snprintf
 *   char buf[json.size() + 16];
 *   snprintf(buf, sizeof(buf), json.c_str(), nodeId);
 */

#include <algorithm>
#include <array>
#include <cstddef>

namespace ct_json {

// ============================================================================
// Core: Fixed-size compile-time string
// ============================================================================

template <size_t N>
struct FixedString {
  std::array<char, N> data{};
  
  consteval FixedString() = default;
  
  consteval FixedString(const char (&str)[N]) {
    std::copy_n(str, N, data.begin());
  }
  
  consteval FixedString(const std::array<char, N>& arr) : data(arr) {}
  
  consteval size_t size() const { return N - 1; }
  consteval const char* c_str() const { return data.data(); }
  consteval char operator[](size_t i) const { return data[i]; }
};

// Deduction guide for string literals
template <size_t N>
FixedString(const char (&)[N]) -> FixedString<N>;

// ============================================================================
// String Concatenation at Compile Time
// ============================================================================

template <size_t N1, size_t N2>
consteval auto concat(const FixedString<N1>& s1, const FixedString<N2>& s2) {
  constexpr size_t NewSize = N1 + N2 - 1;  // -1 for null terminator overlap
  std::array<char, NewSize> result{};
  
  for (size_t i = 0; i < N1 - 1; ++i) result[i] = s1[i];
  for (size_t i = 0; i < N2; ++i) result[N1 - 1 + i] = s2[i];
  
  return FixedString<NewSize>(result);
}

// Variadic concat
template <typename... Strings>
consteval auto concat_all(const Strings&... strings) {
  if constexpr (sizeof...(strings) == 0) {
    return FixedString("");
  } else if constexpr (sizeof...(strings) == 1) {
    return (strings, ...);
  } else {
    return []<typename First, typename... Rest>(const First& first, const Rest&... rest) {
      return concat(first, concat_all(rest...));
    }(strings...);
  }
}

// ============================================================================
// JSON Primitives (Compile-Time)
// ============================================================================

/// JSON string literal: "value"
template <size_t N>
consteval auto string(const char (&str)[N]) {
  constexpr size_t ResultSize = N + 2;  // +2 for quotes
  std::array<char, ResultSize> result{};
  result[0] = '"';
  for (size_t i = 0; i < N - 1; ++i) result[i + 1] = str[i];
  result[N] = '"';
  result[N + 1] = '\0';
  return FixedString<ResultSize>(result);
}

/// JSON number (as string for static use)
template <size_t N>
consteval auto number(const char (&str)[N]) {
  return FixedString(str);
}

/// JSON boolean true
consteval auto bool_true() {
  return FixedString("true");
}

/// JSON boolean false
consteval auto bool_false() {
  return FixedString("false");
}

/// JSON null
consteval auto null() {
  return FixedString("null");
}

/// Placeholder for runtime values (use with snprintf)
/// %d for integers, %s for strings, %.2f for floats
template <size_t N>
consteval auto placeholder(const char (&fmt)[N]) {
  return FixedString(fmt);
}

// Common placeholders
consteval auto placeholder_int() { return FixedString("%d"); }
consteval auto placeholder_uint() { return FixedString("%u"); }
consteval auto placeholder_long() { return FixedString("%ld"); }
consteval auto placeholder_ulong() { return FixedString("%lu"); }
consteval auto placeholder_float() { return FixedString("%.2f"); }
consteval auto placeholder_string() { return FixedString("\"%s\""); }
consteval auto placeholder_raw() { return FixedString("%s"); }

// ============================================================================
// JSON Structure Builders
// ============================================================================

/// JSON key-value pair: "key":value
template <size_t KeyLen, size_t ValLen>
consteval auto pair(const char (&key)[KeyLen], const FixedString<ValLen>& value) {
  return concat_all(
    string(key),
    FixedString(":"),
    value
  );
}

/// JSON object: {pair1,pair2,...}
template <typename... Pairs>
consteval auto object(const Pairs&... pairs) {
  if constexpr (sizeof...(pairs) == 0) {
    return FixedString("{}");
  } else {
    return concat_all(
      FixedString("{"),
      []<typename First, typename... Rest>(const First& first, const Rest&... rest) {
        if constexpr (sizeof...(rest) == 0) {
          return first;
        } else {
          return concat_all(first, FixedString(","), object_inner(rest...));
        }
      }(pairs...),
      FixedString("}")
    );
  }
}

// Helper for object inner content (comma-separated pairs)
template <typename First, typename... Rest>
consteval auto object_inner(const First& first, const Rest&... rest) {
  if constexpr (sizeof...(rest) == 0) {
    return first;
  } else {
    return concat_all(first, FixedString(","), object_inner(rest...));
  }
}

/// JSON array: [item1,item2,...]
template <typename... Items>
consteval auto array(const Items&... items) {
  if constexpr (sizeof...(items) == 0) {
    return FixedString("[]");
  } else {
    return concat_all(
      FixedString("["),
      []<typename First, typename... Rest>(const First& first, const Rest&... rest) {
        if constexpr (sizeof...(rest) == 0) {
          return first;
        } else {
          return concat_all(first, FixedString(","), object_inner(rest...));
        }
      }(items...),
      FixedString("]")
    );
  }
}

// ============================================================================
// Compile-Time JSON Templates for Common Payloads
// ============================================================================

namespace templates {

/// Sensor data payload template (matches ApiClient::createAndCachePayload format)
/// Usage: snprintf(buf, size, SENSOR_PAYLOAD.c_str(), gh_id, node_id, temp, humidity, lux, rssi, timeStr);
/// Format: {"gh_id":%d,"node_id":%d,"temperature":%.1f,"humidity":%.1f,"light_intensity":%u,"rssi":%ld,"recorded_at":"%s"}
inline constexpr auto SENSOR_PAYLOAD = FixedString(
  "{\"gh_id\":%d,\"node_id\":%d,\"temperature\":%.1f,\"humidity\":%.1f,"
  "\"light_intensity\":%u,\"rssi\":%ld,\"recorded_at\":\"%s\"}"
);

/// Sensor data payload size (template + max value lengths)
/// gh_id: 5, node_id: 5, temp: 8, humidity: 8, lux: 5, rssi: 6, time: 19
inline constexpr size_t SENSOR_PAYLOAD_MAX_SIZE = SENSOR_PAYLOAD.size() + 56;

/// Edge mode payload template (gateway-compatible format)
/// Uses "lux" (not "light_intensity") and Unix "timestamp" (not "recorded_at" string)
/// This ensures perfect sync with gateway's SensorDataManager::updateFromNode()
/// Usage: snprintf(buf, size, EDGE_PAYLOAD.c_str(), gh_id, node_id, temp, humidity, lux, rssi, timestamp);
/// Format: {"gh_id":%d,"node_id":%d,"temperature":%.1f,"humidity":%.1f,"lux":%u,"rssi":%ld,"timestamp":%lu}
inline constexpr auto EDGE_PAYLOAD = FixedString(
  "{\"gh_id\":%d,\"node_id\":%d,\"temperature\":%.1f,\"humidity\":%.1f,"
  "\"lux\":%u,\"rssi\":%ld,\"timestamp\":%lu}"
);

inline constexpr size_t EDGE_PAYLOAD_MAX_SIZE = EDGE_PAYLOAD.size() + 48;

/// Status response template
/// Usage: snprintf(buf, size, STATUS_RESPONSE.c_str(), uptimeMs, freeHeap, wifiStatus);
inline constexpr auto STATUS_RESPONSE = FixedString(
  "{\"uptime_ms\":%lu,\"free_heap\":%u,\"wifi_status\":\"%s\"}"
);

/// Error response template
/// Usage: snprintf(buf, size, ERROR_RESPONSE.c_str(), errorCode, errorMsg);
inline constexpr auto ERROR_RESPONSE = FixedString(
  "{\"error\":%d,\"message\":\"%s\"}"
);

/// Simple OK response
inline constexpr auto OK_RESPONSE = FixedString("{\"status\":\"ok\"}");

/// Cache status response
inline constexpr auto CACHE_STATUS = FixedString(
  "{\"size\":%lu,\"head\":%lu,\"tail\":%lu,\"records\":%u}"
);

}  // namespace templates

// ============================================================================
// Buffer Size Calculation
// ============================================================================

/// Calculate required buffer size for a JSON template with runtime values
/// Add max expected length of each placeholder
template <size_t TemplateSize, size_t... PlaceholderMaxLens>
consteval size_t buffer_size() {
  return TemplateSize + (PlaceholderMaxLens + ...);
}

/// Common placeholder max lengths
constexpr size_t MAX_INT_LEN = 11;       // -2147483648
constexpr size_t MAX_UINT_LEN = 10;      // 4294967295
constexpr size_t MAX_LONG_LEN = 20;      // 64-bit
constexpr size_t MAX_FLOAT_LEN = 16;     // -1.79e+308
constexpr size_t MAX_SHORT_STRING = 32;
constexpr size_t MAX_MEDIUM_STRING = 64;
constexpr size_t MAX_SSID_LEN = REDACTED
constexpr size_t MAX_IP_LEN = 15;        // 255.255.255.255

}  // namespace ct_json

#endif  // COMPILE_TIME_JSON_H
