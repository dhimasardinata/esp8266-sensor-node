#include <unity.h>

#define NATIVE_TEST 1
#define NATIVE_TEST_HELPER_IMPLEMENTATION
#include "NativeTestHelper.h"

// Forward declarations from other test units
void test_config_validation();
void test_wifi_store();
void test_json_escaping();
void test_apiclient_payload_fragmentation();
void test_simulated_system_load();
void test_peak_load_simulation();

void setUp(void) {
    mock_networks.clear();
    mock_scan_complete = -2;
    current_millis = 0;
}

void tearDown(void) {}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_config_validation);
    RUN_TEST(test_wifi_store);
    RUN_TEST(test_json_escaping);
    RUN_TEST(test_apiclient_payload_fragmentation);
    RUN_TEST(test_simulated_system_load);
    RUN_TEST(test_peak_load_simulation);
    return UNITY_END();
}
