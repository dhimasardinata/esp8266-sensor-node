#pragma once

#include "Arduino.h"
#undef min
#undef max
#include "LittleFS.h"
#include "FS.h"
#include <iostream>
#include <vector>

// 1. Mock Global Hardware Objects (if not in Arduino.h)
#ifndef ESP_MOCK_DEFINED
#define ESP_MOCK_DEFINED
struct EspClass {
    uint32_t getChipId() { return 0x12345678; }
    uint32_t getFreeHeap() { return 20000; }
    void wdtFeed() {}
};
// Use 'inline' or 'extern' to avoid duplicate definition if included multiple times
// But in a single .cpp test file, static/global is fine.
// For header, we should declare extern and define in one place, or use inline.
inline EspClass ESP;
#endif

// 2. Define global symbols required by Dependencies
#ifdef _WIN32
  #ifdef _timezone
    #undef _timezone
  #endif
#endif
#ifndef NATIVE_MOCK_TIMEZONE
#define NATIVE_MOCK_TIMEZONE
static long _timezone = 0;
#endif

// 3. Helper to include implementations
// Using macros or Just direct include
// We need Utils for ApiClient.
// We assume this header is included FROM a test file in test/test_native_stress/

// Include Mocks First
#include "bearssl/bearssl_hash.h"

// Include Headers
#include "CryptoUtils.h"
#include "utils.h"

// Include Implementations (only once per test binary)
#ifdef NATIVE_TEST_HELPER_IMPLEMENTATION
// Minimal stub to satisfy native linking without pulling full crypto stack.
namespace CryptoUtils {
inline size_t fast_serialize_encrypted(std::string_view plaintext, char* out_buf, size_t out_len) {
    if (!out_buf || out_len == 0) {
        return 0;
    }
    size_t n = plaintext.size();
    if (n > (out_len - 1)) {
        n = out_len - 1;
    }
    if (n > 0) {
        memcpy(out_buf, plaintext.data(), n);
    }
    out_buf[n] = '\0';
    return n;
}
}  // namespace CryptoUtils

#include "utils.cpp"
#include "Logger.cpp"
#include "ConfigManager.cpp"
#include "REDACTED"
#endif
