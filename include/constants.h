// File: include/constants.h

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <Arduino.h>

// Menggunakan namespace untuk mencegah polusi global dan konflik nama
namespace AppConstants {

  // =========================================================================
  // == WiFi & Network
  // =========================================================================
  constexpr uint16_t DNS_PORT = 53;
  constexpr size_t MAX_WS_CLIENTS = 4;

  // --- Rate Limiting untuk Login & OTA (dalam ms) ---
  constexpr int MAX_FAILED_AUTH_ATTEMPTS = 5;
  constexpr unsigned long AUTH_LOCKOUT_DURATION_MS = 5 * 60 * 1000;  // 5 menit

  // =========================================================================
  // == Application Timers (dalam milidetik)
  // =========================================================================
  constexpr unsigned long SENSOR_STABILIZATION_DELAY_MS = 2000;
  constexpr unsigned long REBOOT_DELAY_MS = 1000;
  constexpr unsigned long LOOP_WDT_TIMEOUT_MS = 30000UL;

  // --- WifiManager Timers ---
  constexpr unsigned long PORTAL_SCAN_TIMER_MS = 30000;
  constexpr unsigned long PORTAL_TEST_TIMER_MS = 20000;
  constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
  constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = 15000;
  constexpr unsigned long INITIAL_CONNECT_WDT_MS = 15 * 60 * 1000;  // 15 menit
  constexpr unsigned long DISCONNECT_WDT_MS = 30 * 60 * 1000;       // 30 menit
  constexpr unsigned long SCHEDULED_REBOOT_MS = 3000;

  // --- NTP Timers ---
  constexpr unsigned long NTP_INITIAL_DELAY_MS = 2000;
  constexpr unsigned long NTP_RETRY_INTERVAL_MS = 5 * 60 * 1000;  // 5 menit
  constexpr unsigned long NTP_SYNC_TIMEOUT_MS = 30000;
  constexpr long TIMEZONE_OFFSET_SEC = 7 * 3600;  // UTC+7 (WIB)

  // --- OTA Timers ---
  constexpr unsigned long OTA_INITIAL_UPDATE_DELAY_MS = 2 * 60 * 1000;          // 2 menit
  constexpr unsigned long OTA_REGULAR_UPDATE_INTERVAL_MS = 1 * 60 * 60 * 1000;  // 1 jam

  // =========================================================================
  // == Sensor Configuration
  // =========================================================================
  constexpr uint8_t BH1750_I2C_ADDR = 0x23;
  constexpr unsigned long SHT_READ_INTERVAL_MS = 2000;
  constexpr long SENSOR_INIT_RETRY_INTERVAL_MS = 250;
  constexpr long SENSOR_SLOW_RETRY_INTERVAL_MS = 5000;
  constexpr long SENSOR_RECOVERY_INTERVAL_MS = 10 * 60 * 1000;  // 10 menit
  constexpr int SENSOR_MAX_FAILURES = 20;

  // =========================================================================
  // == Common Delays (avoid magic numbers)
  // =========================================================================
  constexpr unsigned long I2C_SETTLE_DELAY_MS = 100;
  constexpr unsigned long BH1750_INIT_DELAY_MS = 200;
  constexpr unsigned long REBOOT_SETTLE_DELAY_MS = 1000;

  // =========================================================================
  // == Session & Security (HARDENING)
  // =========================================================================
  constexpr unsigned long WS_SESSION_TIMEOUT_MS = 30 * 60 * 1000;  // 30 minutes inactivity logout
  constexpr unsigned long WS_SESSION_CHECK_INTERVAL_MS = 60 * 1000;  // Check every minute

  // =========================================================================
  // == Memory Monitoring (TOOLING)
  // =========================================================================
  constexpr uint32_t HEAP_WARNING_THRESHOLD = 8192;  // Warn when free heap below 8KB
  constexpr uint32_t HEAP_CRITICAL_THRESHOLD = 4096; // Critical when below 4KB
  constexpr uint8_t FRAGMENTATION_WARNING_PERCENT = 40;  // Warn when fragmentation > 40%

  // API Client Memory Safety Thresholds
  constexpr uint32_t API_MIN_SAFE_BLOCK_SIZE = 2500;  // Minimum contiguous memory block for HTTP
  constexpr uint32_t API_MIN_TOTAL_HEAP = 4096;       // Minimum total free heap for HTTP

  // =========================================================================
  // == Input Validation Bounds (HARDENING)
  // =========================================================================
  constexpr float CALIBRATION_OFFSET_MAX = 50.0f;  // Max calibration offset
  constexpr float LUX_FACTOR_MAX = 10.0f;  // Max lux scaling factor
  constexpr unsigned long INTERVAL_MIN_MS = 1000UL;  // Minimum interval (1 second)
  constexpr unsigned long INTERVAL_MAX_MS = 24UL * 60 * 60 * 1000;  // Max interval (24 hours)

  // =========================================================================
  // == Terminal & WebSocket Limits (DRY/SECURITY)
  // =========================================================================
  constexpr size_t MAX_WS_PACKET_SIZE = 512;     // Max WebSocket packet size
  constexpr size_t MAX_COMMAND_ARGS = 16;        // Max arguments per command
  constexpr size_t INPUT_SANITIZE_MAX_LEN = 256; // Max input after sanitization
  constexpr size_t TERMINAL_LINE_WIDTH = 40;     // Default terminal output width

}  // namespace AppConstants

#endif  // CONSTANTS_H