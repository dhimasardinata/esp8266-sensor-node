#include <ApiClient.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "mock_cache_manager.h"
#include "mock_sensor_manager.h"

// --- Mock other dependencies ---
AsyncWebSocket mock_ws("/ws");
BearSSL::WiFiClientSecure mock_secure_client;
ConfigManager mock_config_manager;
WifiManager mock_wifi_manager;
NtpClient mock_ntp_client(mock_wifi_manager);

// --- Instantiate our Mocks ---
MockSensorManager mock_sensor_manager;
MockCacheManager mock_cache_manager;

void setUp(void) {
  // Reset mocks before each test
  mock_sensor_manager = MockSensorManager();
  mock_cache_manager.reset();
  // Set a default valid time for tests
  setTime(1704067201);  // 2024-01-01 00:00:01
}

void tearDown(void) {}

void test_createAndCachePayload_writes_correct_json_for_valid_data() {
  // 1. ARRANGE
  // Init config with default values
  mock_config_manager.init();

  // Configure mock sensors
  mock_sensor_manager.mockTemp = {25.5f, true};
  mock_sensor_manager.mockHum = {60.2f, true};
  mock_sensor_manager.mockLight = {5000.0f, true};

  // Instantiate the class under test, injecting all mocks
  ApiClient apiClient(mock_ws,
                      mock_ntp_client,
                      mock_wifi_manager,
                      mock_sensor_manager,
                      mock_secure_client,
                      mock_config_manager,
                      mock_cache_manager);

  // 2. ACT
  // The method to test is private, so we test it via a public method that calls it.
  apiClient.scheduleImmediateUpload();

  // 3. ASSERT
  // Check that the mock cache received data
  TEST_ASSERT_EQUAL_INT(1, mock_cache_manager.stored_data.size());

  // Parse the JSON that was "cached"
  StaticJsonDocument<256> doc;
  const std::string& payload = mock_cache_manager.stored_data.front();
  DeserializationError error = deserializeJson(doc, payload);

  TEST_ASSERT_EQUAL(DeserializationError::Ok, error);
  TEST_ASSERT_EQUAL_FLOAT(25.5, doc["temperature"]);
  TEST_ASSERT_EQUAL_FLOAT(60.2, doc["humidity"]);
  TEST_ASSERT_EQUAL_INT(5000, doc["light_intensity"]);
  TEST_ASSERT_EQUAL_STRING("2024-01-01 00:00:01", doc["recorded_at"]);
}

void test_createAndCachePayload_writes_zero_for_invalid_data() {
  // 1. ARRANGE
  mock_config_manager.init();
  mock_sensor_manager.mockTemp = {-999.0f, false};
  mock_sensor_manager.mockHum = {-1.0f, false};
  mock_sensor_manager.mockLight = {-1.0f, false};

  ApiClient apiClient(mock_ws,
                      mock_ntp_client,
                      mock_wifi_manager,
                      mock_sensor_manager,
                      mock_secure_client,
                      mock_config_manager,
                      mock_cache_manager);

  // 2. ACT
  apiClient.scheduleImmediateUpload();

  // 3. ASSERT
  TEST_ASSERT_EQUAL_INT(1, mock_cache_manager.stored_data.size());
  StaticJsonDocument<256> doc;
  deserializeJson(doc, mock_cache_manager.stored_data.front());

  TEST_ASSERT_EQUAL_FLOAT(0.0, doc["temperature"]);
  TEST_ASSERT_EQUAL_FLOAT(0.0, doc["humidity"]);
  TEST_ASSERT_EQUAL_INT(0, doc["light_intensity"]);
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_createAndCachePayload_writes_correct_json_for_valid_data);
  RUN_TEST(test_createAndCachePayload_writes_zero_for_invalid_data);
  UNITY_END();
}

void loop() {}