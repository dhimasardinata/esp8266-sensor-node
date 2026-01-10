#pragma once
#include "Arduino.h"

// Mock LittleFS mainly for CacheManager usage if needed, or AppServer
class FS {
public:
    bool begin() { return true; }
    void end() {}
    bool format() { return true; }
};
extern FS LittleFS;
