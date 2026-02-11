#include <unity.h>
#include <iostream>
#include <vector>
#include <string.h>

#define NATIVE_TEST 1

#include "NativeTestHelper.h"
#include "ConfigManager.h"
#include "REDACTED"

// ============================================================================
// TEST 1: CONFIG VALIDATION
// ============================================================================
void test_config_validation() {
    LittleFS.format();
    ConfigManager cfgMgr;
    cfgMgr.init();
    
    AppConfig temp = cfgMgr.getConfig();

    // Test 1: Clamp Low Values
    temp.DATA_UPLOAD_INTERVAL_MS = 10;
    temp.SENSOR_SAMPLE_INTERVAL_MS = 10;
    temp.CACHE_SEND_INTERVAL_MS = 10;
    temp.SOFTWARE_WDT_TIMEOUT_MS = 10;

    cfgMgr.setTimingConfig(temp);

    const AppConfig& cfg = cfgMgr.getConfig();
    TEST_ASSERT_EQUAL_UINT32(5000, cfg.DATA_UPLOAD_INTERVAL_MS);
    TEST_ASSERT_EQUAL_UINT32(1000, cfg.SENSOR_SAMPLE_INTERVAL_MS);
    TEST_ASSERT_EQUAL_UINT32(1000, cfg.CACHE_SEND_INTERVAL_MS);
    TEST_ASSERT_EQUAL_UINT32(60000, cfg.SOFTWARE_WDT_TIMEOUT_MS);
    
    printf("[TEST] Config Clamping: OK\n");
}

// ============================================================================
// TEST 2: WIFI CREDENTIAL STORE
// ============================================================================
void test_wifi_store() {
    LittleFS.format();
    WifiCredentialStore store;
    store.init();
    
    // Test 1: Add Credential
    TEST_ASSERT_TRUE(store.addCredential("REDACTED", "REDACTED"));
    TEST_ASSERT_TRUE(store.hasCredential("REDACTED"));
    
    // Test 2: Retrieval
    const WifiCredential* found = REDACTED
    for (const auto& cred : store.getSavedCredentials()) {
        if (!cred.isEmpty() && strcmp(cred.ssid, "HomeWiFi") =REDACTED
            found = &cred;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("REDACTED", found->ssid);
    TEST_ASSERT_EQUAL_STRING("REDACTED", found->password);
    
    // Test 3: Remove
    TEST_ASSERT_TRUE(store.removeCredential("REDACTED"));
    TEST_ASSERT_FALSE(store.hasCredential("REDACTED"));
    
    printf("[TEST] WifiStore CRUD: REDACTED
}

// ============================================================================
// TEST 3: JSON ESCAPING (SECURITY)
// ============================================================================
void test_json_escaping() {
    char buf[64];
    
    // Normal string
    TEST_ASSERT_GREATER_THAN(0, Utils::escape_json_string(std::span<char>(buf, sizeof(buf)), "Hello"));
    TEST_ASSERT_EQUAL_STRING("Hello", buf);
    
    // Quote injection
    TEST_ASSERT_GREATER_THAN(0, Utils::escape_json_string(std::span<char>(buf, sizeof(buf)), "He\"llo"));
    TEST_ASSERT_EQUAL_STRING("He\\\"llo", buf);
    
    // Control char
    TEST_ASSERT_GREATER_THAN(0, Utils::escape_json_string(std::span<char>(buf, sizeof(buf)), "Line\nBreak"));
    TEST_ASSERT_EQUAL_STRING("Line\\nBreak", buf);
    
    printf("[TEST] JSON Escaping: OK\n");
}

