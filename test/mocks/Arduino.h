#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <algorithm>

// Arduino String Shim
class String : public std::string {
public:
    String() : std::string() {}
    String(const char* s) : std::string(s ? s : "") {}
    String(const std::string& s) : std::string(s) {}
    String(int i) : std::string(std::to_string(i)) {}
    
    unsigned char operator[](unsigned int index) const {
        return at(index);
    }
    
    bool startsWith(const String& prefix) const {
        return rfind(prefix, 0) == 0;
    }
    bool endsWith(const String& suffix) const {
        if (length() < suffix.length()) return false;
        return compare(length() - suffix.length(), suffix.length(), suffix) == 0;
    }
    
    // Concatenation
    String& operator+=(const String& rhs) { append(rhs); return *this; }
    
    const char* c_str() const { return std::string::c_str(); }

    void toCharArray(char* buf, size_t len) const {
        if (!buf || len == 0) return;
        size_t n = std::min(len - 1, this->size());
        memcpy(buf, this->c_str(), n);
        buf[n] = '\0';
    }
};

// F() Macro for PROGMEM (No-Op on Native)
class __FlashStringHelper;
#define F(x) (const __FlashStringHelper*)(x)
#define PSTR(x) x
#define PROGMEM 
#define vsnprintf_P vsnprintf
#define strcpy_P strcpy
#define strncpy_P strncpy
#define pgm_read_dword(ptr) (*(const uint32_t*)(ptr))
#define pgm_read_ptr(ptr) (*(const void**)(ptr))
using PGM_P = const char*;

template <typename T>
inline T max(T a, T b) { return (a > b) ? a : b; }
template <typename T>
inline T min(T a, T b) { return (a < b) ? a : b; }

inline void noInterrupts() {}
inline void interrupts() {}
inline void yield() {}

// Print Interface Mock
class Print {
public:
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) {
        size_t n = 0;
        while (size--) {
            if (write(*buffer++)) n++;
            else break;
        }
        return n;
    }
    
    size_t print(const String &s) { return write((const uint8_t*)s.c_str(), s.length()); }
    size_t print(const char* str) { return write((const uint8_t*)str, strlen(str)); }
    size_t print(int n) { return print(String(n)); }
    
    size_t printf(const char *format, ...) {
        char buf[1024];
        va_list args;
        va_start(args, format);
        int len = vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        return write((const uint8_t*)buf, len);
    }

    size_t printf_P(const char *format, ...) {
        char buf[1024];
        va_list args;
        va_start(args, format);
        int len = vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        return write((const uint8_t*)buf, len);
    }
};

// WiFi Mock State (inline to avoid multiple definitions across test TUs)
inline int mock_scan_complete = -2;
inline std::vector<std::pair<String, int>> mock_networks;

// WiFi Class Mock
class WiFiClass {
public:
    int scanComplete() { return mock_scan_complete; }
    void scanNetworks(bool) {}
    String SSID(int i) {
        if (i >= 0 && i < (int)mock_networks.size()) return mock_networks[i].first;
        return "";
    }
    int32_t RSSI(int i) {
        if (i >= 0 && i < (int)mock_networks.size()) return mock_networks[i].second;
        return -100;
    }
    int encryptionType(int) { return 0; } // ENC_TYPE_NONE = 7 (Open) typically, but here 0 is fine for simplicity or we define enum
    
    String SSID() { return "REDACTED"; }
    String localIP() { return "192.168.1.100"; }
};

inline WiFiClass WiFi;

// Enum for Encryption
enum { ENC_TYPE_NONE = 7 };

// Stub out millis
inline uint32_t current_millis = 0;
inline uint32_t millis() { return current_millis; }
inline void delay(uint32_t) {}

// Stub out Serial
class SerialMock : public Print {
public:
    size_t write(uint8_t c) override {
        std::cout << (char)c;
        return 1;
    }
    void begin(int) {}
    size_t println(const char* s) { size_t n = print(s); write('\n'); return n + 1; }
    size_t println(const String& s) { size_t n = print(s); write('\n'); return n + 1; }
    size_t println() { write('\n'); return 1; }
};
inline SerialMock Serial;
