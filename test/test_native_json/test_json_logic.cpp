#include <unity.h>
#include "Arduino.h"
#include "ESPAsyncWebServer.h"

// --- DUT (Device Under Test) LOGIC ---
// We copy the logic from PortalServer::sendNetworksJson to verify it.
// In a real scenario, this logic would be in a decoupled static function or helper class.

// Mock Credential Store
struct MockCredentialStore {
    bool hasCredential(const char*) { return false; }
};
struct MockWifiManager {
    MockCredentialStore getCredentialStore() { return MockCredentialStore(); }
};

void logic_sendNetworksJson(AsyncWebServerRequest* request, MockWifiManager& m_wifiManager) {
  int n = WiFi.scanComplete();
  // WIFI_SCAN_RUNNING is usually -1, WIFI_SCAN_FAILED is -2
  const int WIFI_SCAN_RUNNING = REDACTED
  
  if (n == WIFI_SCAN_RUNNING) {
    request->send(200, "application/json", "{\"scanning\":true}");
    return;
  }

  if (n < 0) {
    WiFi.scanNetworks(true);
    request->send(200, "application/json", "{\"scanning\":true}");
    return;
  }

  AsyncResponseStream* response = request->beginResponseStream("application/json");
  response->print("{\"networks\":[");
  
  bool first = true;
  for (int i = 0; i < n; i++) {
    if (!first) response->print(",");
    first = false;

    String ssid = REDACTED
    int32_t rssi = WiFi.RSSI(i);
    bool isOpen = (WiFi.encryptionType(i) == ENC_TYPE_NONE);

    int bars = 0;
    if (rssi > -50) bars = 4;
    else if (rssi > -60) bars = 3;
    else if (rssi > -70) bars = 2;
    else if (rssi > -80) bars = 1;

    bool isKnown = m_wifiManager.getCredentialStore().hasCredential(ssid.c_str());

    response->printf("{\"ssid\":REDACTED
      ssid.c_str(), rssi, bars, isOpen ? "true" : REDACTED
  }

  response->print("]}");
  request->send(response);
}

// --- TESTS ---

void setUp(void) {
    mock_networks.clear();
    mock_scan_complete = -2;
}

void tearDown(void) {}

void test_json_streaming_multiple_networks(void) {
    // 1. Setup Mock Data
    mock_networks.push_back({"REDACTED", -55}); // 3 bars
    mock_networks.push_back({"REDACTED", -85}); // 0 bars
    mock_scan_complete = 2; // 2 networks found
    
    MockWifiManager wifiMgr;
    AsyncWebServerRequest request;
    
    // 2. Execute Logic
    logic_sendNetworksJson(&request, wifiMgr);
    
    // 3. Verify
    TEST_ASSERT_NOT_NULL(request._tempStream);
    const std::string& output = request._tempStream->content;
    
    printf("Generated JSON: %s\n", output.c_str());
    
    // Assert structure
    TEST_ASSERT_TRUE(output.find("{\"networks\":[") == 0);
    TEST_ASSERT_TRUE(output.find("{\"ssid\":REDACTED
    TEST_ASSERT_TRUE(output.find("{\"ssid\":REDACTED
    TEST_ASSERT_TRUE(output.find("]}") == output.length() - 2);
}

void test_json_scanning_state(void) {
    mock_scan_complete = -1; // Scanning
    MockWifiManager wifiMgr;
    AsyncWebServerRequest request;
    
    logic_sendNetworksJson(&request, wifiMgr);
    
    TEST_ASSERT_NULL(request._tempStream);
    // In a real mock we'd verify request->send(...) was called with specific string, 
    // but here we know beginResponseStream wasn't called.
}

void test_stress_ram_large_dataset(void) {
    // Simulate 200 Networks (Extreme Case)
    // 200 * ~80 bytes = ~16KB payload.
    // On ESP8266, a single predictable 16KB allocation is SAFE.
    // The danger was String Reallocation Fragmentation (allocating 100, then 200, then 300...).
    
    mock_networks.clear();
    for(int i=0; i<200; i++) {
        String ssid = REDACTED
        mock_networks.push_back({ssid, -60});
    }
    mock_scan_complete = 200;

    MockWifiManager wifiMgr;
    AsyncWebServerRequest request;

    Serial.printf("\n[STRESS] Generating JSON for 200 networks...\n");
    
    // Measure "Heap" (Content Size)
    logic_sendNetworksJson(&request, wifiMgr);

    TEST_ASSERT_NOT_NULL(request._tempStream);
    size_t finalSize = request._tempStream->content.length();
    Serial.printf("[STRESS] Final Payload Size: %zu bytes\n", finalSize);
    
    // Assert reasonable size (approx 200 * 90 chars ~ 18KB)
    // If we were using String+=, this might have triggered 200 reallocations.
    // AsyncResponseStream (std::vector based) typically reallocates log(N) times.
    TEST_ASSERT_GREATER_THAN(15000, finalSize); 
    TEST_ASSERT_LESS_THAN(25000, finalSize);

    // Verify content integrity at end
    TEST_ASSERT_TRUE(request._tempStream->content.find("ExtremelyLongSSIDNameToSendTheUsageUp_199") != REDACTED
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_json_streaming_multiple_networks);
    RUN_TEST(test_json_scanning_state);
    RUN_TEST(test_stress_ram_large_dataset);
    return UNITY_END();
}
