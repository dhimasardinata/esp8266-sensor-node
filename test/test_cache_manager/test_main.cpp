// File: test/test_integration/test_main.cpp

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <unity.h>

// Include header dari kode produksi yang kita butuhkan
#include <ApiClient.h>
#include <ConfigManager.h>
#include <ESPAsyncWebServer.h>
#include <NtpClient.h>
#include <SensorManager.h>
#include <WiFiClientSecureBearSSL.h>
#include <WifiManager.h>
#include <cache_manager.h>

// Deklarasi objek dummy eksternal
extern AsyncWebServer g_dummyServer;
extern AsyncWebSocket g_dummyWs;
extern ConfigManager g_dummyConfig;

// Objek global untuk tes
ConfigManager g_configManager;
SensorManager g_sensorManager;
WifiManager g_wifiManager(g_dummyServer, g_dummyWs, g_dummyConfig);
NtpClient g_ntpClient(g_wifiManager);
BearSSL::WiFiClientSecure g_secureClient;

ApiClient g_apiClient(g_ntpClient, g_wifiManager, g_sensorManager, g_secureClient, g_configManager);

void setUp(void) {
  if (!LittleFS.begin()) {
    LittleFS.format();
    LittleFS.begin();
  }
  g_configManager.init();  // Ini akan memuat nilai default dari calibration.h
  g_sensorManager.init();
  g_apiClient.applyConfig(g_configManager.getConfig());
  cache_manager_init();
  cache_manager_reset();
}

void tearDown(void) {
  LittleFS.end();
}

void test_payload_creation_with_valid_data_does_not_crash(void) {
  Serial.println(F("\n[TEST] Memaksa pembuatan payload seolah-olah sensor valid (via Linker Seam)..."));

  bool success = g_apiClient.sendCurrentDataNow();

  TEST_ASSERT_TRUE_MESSAGE(success, "g_apiClient.sendCurrentDataNow() should return true");

  Serial.println(F("[TEST] Payload berhasil dibuat tanpa crash."));

  uint32_t cache_size = cache_manager_get_size();
  TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0, cache_size, "Cache should not be empty");

  std::vector<char> buffer;
  cache_manager_read_one(buffer);
  std::string payload(buffer.begin(), buffer.end());

  Serial.printf("[TEST] Isi payload dari cache: %s\n", payload.c_str());

  // --- PERBAIKAN: Buat ekspektasi dinamis berdasarkan kalibrasi ---

  // 1. Dapatkan konfigurasi yang sedang berjalan (yang dimuat di setUp)
  const auto& cfg = g_configManager.getConfig();

  // 2. Tentukan nilai mentah yang kita tahu berasal dari mock_dependencies.cpp
  const float raw_temp = 25.5f;
  const float raw_humidity = 60.2f;

  // 3. Hitung nilai akhir yang diharapkan
  float expected_temp = raw_temp + cfg.TEMP_OFFSET;
  float expected_humidity = raw_humidity + cfg.HUMIDITY_OFFSET;

  // 4. Buat string ekspektasi untuk dicari di dalam JSON
  char expected_temp_str[32];
  char expected_hum_str[32];
  snprintf(expected_temp_str, sizeof(expected_temp_str), "\"temperature\":\"%.1f\"", expected_temp);
  snprintf(expected_hum_str, sizeof(expected_hum_str), "\"humidity\":\"%.1f\"", expected_humidity);

  // 5. Verifikasi menggunakan string dinamis tersebut
  TEST_ASSERT_TRUE_MESSAGE(payload.find(expected_temp_str) != std::string::npos,
                           "Payload should contain the correctly calibrated temperature");
  TEST_ASSERT_TRUE_MESSAGE(payload.find(expected_hum_str) != std::string::npos,
                           "Payload should contain the correctly calibrated humidity");

  Serial.println("[VERIFIKASI] Nilai suhu dan kelembaban terkalibrasi dengan benar.");
}

void setup() {
  delay(2000);
  Serial.begin(115200);
  UNITY_BEGIN();

  RUN_TEST(test_payload_creation_with_valid_data_does_not_crash);

  UNITY_END();
}

void loop() {}