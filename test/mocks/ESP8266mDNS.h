#pragma once

class MDNSResponder {
public:
    bool begin(const char*) { return true; }
    void update() {}
    void addService(const char*, const char*, int) {}
    void close() {}
};
extern MDNSResponder MDNS;
