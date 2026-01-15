#pragma once

#include "Arduino.h"
#include <string>
#include <sstream>

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
