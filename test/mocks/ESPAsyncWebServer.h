#pragma once

#include "Arduino.h"
#include <string>
#include <sstream>
#include <functional>

// WebSocket minimal stubs
enum AwsEventType { WS_EVT_CONNECT, WS_EVT_DISCONNECT, WS_EVT_DATA, WS_EVT_ERROR };
struct AwsFrameInfo {
    uint8_t opcode = 0;
    size_t index = 0;
    size_t len = 0;
    bool final = true;
};
constexpr uint8_t WS_TEXT = 0x1;

class AsyncWebSocketClient {
public:
    bool canSend() const { return true; }
    void text(const char*) {}
    uint32_t id() const { return 0; }
};

class AsyncWebSocket {
public:
    explicit AsyncWebSocket(const char* = nullptr) {}
    AsyncWebSocketClient* client(uint32_t) { return nullptr; }
    template <typename T>
    void onEvent(T) {}
};

class AsyncResponseStream : public Print {
public:
    std::string content;
    
    size_t write(uint8_t c) override {
        content += (char)c;
        return 1;
    }
    
    void setContentType(const String& type) {}
};

class AsyncWebServerRequest {
public:
    AsyncResponseStream* _tempStream = nullptr;

    AsyncResponseStream* beginResponseStream(const String& contentType) {
        _tempStream = new AsyncResponseStream();
        return _tempStream;
    }
    
    void send(AsyncResponseStream* stream) {
        // In native test, we assume ownership implies success.
        // The test will inspect 'stream->content'
    }
    
    void send(int code, const String& mime, const String& content) {
        // Simple send implementation for mocking
    }
    
    ~AsyncWebServerRequest() {
        if (_tempStream) delete _tempStream;
    }
};
