#pragma once

#include "Arduino.h"
#include "ESPAsyncWebServer.h"

// --- HTTPClient Mock ---
class HTTPClient {
public:
    static int mock_response_code;
    
    void begin(const String& url) {}
    // Overload for SecureClient (signature match)
    bool begin(void* client, const String& url) { return true; } 
    
    void setReuse(bool) {}
    void setTimeout(int) {}
    void addHeader(const String& name, const String& value) {}
    void collectHeaders(const char* headerKeys[], const size_t count) {}
    void setFollowRedirects(int) {}
    
    int POST(const String& payload) { return mock_response_code; }
    int POST(uint8_t* payload, size_t size) { return mock_response_code; }
    int GET() { return mock_response_code; }
    
    String getString() { return "{\"mock\":\"response\"}"; }
    String header(const String& name) { return ""; }
    String getLocation() { return ""; }
    String errorToString(int code) { return "Mock Error"; }
    
    void end() {}
};
int HTTPClient::mock_response_code = 200;

#define HTTPC_ERROR_CONNECTION_FAILED -1
#define HTTPC_DISABLE_FOLLOW_REDIRECTS 0

// --- NtpClient Mock ---
class NtpClient {
public:
    bool _synced = false;
    bool isTimeSynced() { return _synced; }
    void setManualTime(time_t t) { _synced = true; }
};

// --- ConfigManager Mock (Minimal) ---
struct AppConfig {
    String DATA_UPLOAD_URL = "http://mock-api.com";
    String AUTH_TOKEN = "mock-token";
    String FW_VERSION_CHECK_URL_BASE = "http://ota.com/";
    unsigned long DATA_UPLOAD_INTERVAL_MS = 60000;
    unsigned long SENSOR_SAMPLE_INTERVAL_MS = 5000;
    unsigned long CACHE_SEND_INTERVAL_MS = 10000;
    unsigned long SOFTWARE_WDT_TIMEOUT_MS = 30000;
    
    float TEMP_OFFSET = 0.0;
    float HUMIDITY_OFFSET = 0.0;
};

class ConfigManager {
public:
    AppConfig _config;
    const AppConfig& getConfig() { return _config; }
    String getHostname() { return "node-test"; }
};

// --- Timer Mock ---
// In native test, we can manually "tick" time
class SimpleTimer {
public:
    unsigned long _last = 0;
    unsigned long _interval = 1000;
    
    void setInterval(unsigned long ms) { _interval = ms; }
    void reset() { _last = millis(); }
    bool hasElapsed() {
        if (millis() - _last >= _interval) {
            _last = millis();
            return true;
        }
        return false;
    }
};

// Redirect original Ticker/IntervalTimer usages if flexible, 
// OR just define them here if not defined elsewhere.
// Assuming ApiClient uses a custom timer class or standard Ticker?
// ApiClient uses member `m_dataCreationTimer` which suggests a custom class or common util.
// Let's assume it matches the interface of SimpleTimer above.

class MockSystemConfig {
public:
    void init() {}
};

