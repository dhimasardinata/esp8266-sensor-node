// File: include/constants.h

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <Arduino.h>

// Use namespace to prevent global namespace pollution and naming conflicts.
#ifndef DEVICE_TIMEZONE_OFFSET_SEC
#define DEVICE_TIMEZONE_OFFSET_SEC (7 * 3600L)
#endif

namespace AppConstants {

  // =========================================================================
  // == WiFi & Network
  // =========================================================================
  constexpr uint16_t DNS_PORT = 53;
  constexpr size_t MAX_WS_CLIENTS = 1;

  // --- Rate Limiting for Login & OTA (in ms) ---
  constexpr int MAX_FAILED_AUTH_ATTEMPTS = REDACTED
  constexpr unsigned long AUTH_LOCKOUT_DURATION_MS = REDACTED // 5 minutes

  // =========================================================================
  // == Application Timers (in milliseconds)
  // =========================================================================
  constexpr unsigned long SENSOR_STABILIZATION_DELAY_MS = 2000;
  constexpr unsigned long REBOOT_DELAY_MS = 1000;
  constexpr unsigned long LOOP_WDT_TIMEOUT_MS = 30000UL;

  // --- WifiManager Timers ---
  constexpr unsigned long PORTAL_SCAN_TIMER_MS = 30000;
  constexpr unsigned long PORTAL_TEST_TIMER_MS = 20000;
  constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = REDACTED
  constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = REDACTED
  constexpr unsigned long INITIAL_CONNECT_WDT_MS = 15 * 60 * 1000;  // 15 minutes
  constexpr unsigned long DISCONNECT_WDT_MS = 30 * 60 * 1000;       // 30 minutes
  constexpr unsigned long SCHEDULED_REBOOT_MS = 3000;

  // --- NTP Timers ---
  constexpr unsigned long NTP_INITIAL_DELAY_MS = 2000;
  constexpr unsigned long NTP_RETRY_INTERVAL_MS = 5 * 60 * 1000;  // 5 minutes
  constexpr unsigned long NTP_SYNC_TIMEOUT_MS = 30000;
  constexpr long TIMEZONE_OFFSET_SEC = DEVICE_TIMEZONE_OFFSET_SEC;  // Default UTC+7 (WIB)

  // --- OTA Timers ---
  constexpr unsigned long OTA_INITIAL_UPDATE_DELAY_MS = REDACTED // 2 minutes
  constexpr unsigned long OTA_REGULAR_UPDATE_INTERVAL_MS = REDACTED // 1 hour

  // =========================================================================
  // == Sensor Configuration
  // =========================================================================
  constexpr uint8_t BH1750_I2C_ADDR = 0x23;
  constexpr unsigned long SHT_READ_INTERVAL_MS = 2000;
  constexpr long SENSOR_INIT_RETRY_INTERVAL_MS = 250;
  constexpr long SENSOR_SLOW_RETRY_INTERVAL_MS = 5000;
  constexpr long SENSOR_RECOVERY_INTERVAL_MS = 10 * 60 * 1000;  // 10 minutes
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
  constexpr unsigned long WS_SESSION_TIMEOUT_MS = 30 * 60 * 1000;    // 30 minutes inactivity logout
  constexpr unsigned long WS_SESSION_CHECK_INTERVAL_MS = 60 * 1000;  // Check every minute

  // =========================================================================
  // == Memory Monitoring (TOOLING)
  // =========================================================================
  constexpr uint32_t HEAP_WARNING_THRESHOLD = 8192;      // Warn when free heap below 8KB
  constexpr uint32_t HEAP_CRITICAL_THRESHOLD = 4096;     // Critical when below 4KB
  constexpr uint8_t FRAGMENTATION_WARNING_PERCENT = 40;  // Warn when fragmentation > 40%

  // API Client Memory Safety Thresholds
  // Keep HTTP guard minimal; TLS has its own stricter guard.
  constexpr uint32_t API_MIN_SAFE_BLOCK_SIZE = 1536;  // Minimum contiguous memory block for HTTP
  constexpr uint32_t API_MIN_TOTAL_HEAP = REDACTED // Minimum total free heap for HTTP
  // TLS handshake needs more heap; keep uploads/OTA from OOM.
  // Keep guard minimal but safe for BearSSL validator allocation.
  constexpr uint32_t TLS_MIN_SAFE_BLOCK_SIZE = 3600;   // Minimum contiguous block for TLS
  constexpr uint32_t TLS_MIN_TOTAL_HEAP = REDACTED // Minimum total free heap for TLS

  // TLS buffer sizes (on-demand)
  constexpr uint16_t TLS_RX_BUF_SIZE = 768;
  constexpr uint16_t TLS_TX_BUF_SIZE = 512;
  constexpr uint16_t TLS_RX_BUF_PORTAL = 512;
  constexpr uint16_t TLS_TX_BUF_PORTAL = 256;

  // =========================================================================
  // == Input Validation Bounds (HARDENING)
  // =========================================================================
  constexpr float CALIBRATION_OFFSET_MAX = 50.0f;                   // Max calibration offset
  constexpr float LUX_FACTOR_MAX = 10.0f;                           // Max lux scaling factor
  constexpr unsigned long INTERVAL_MIN_MS = 1000UL;                 // Minimum interval (1 second)
  constexpr unsigned long INTERVAL_MAX_MS = 24UL * 60 * 60 * 1000;  // Max interval (24 hours)

  // =========================================================================
  // == Terminal & WebSocket Limits (DRY/SECURITY)
  // =========================================================================
  constexpr size_t MAX_WS_PACKET_SIZE = 248;      // Max WebSocket packet size
  constexpr size_t MAX_COMMAND_ARGS = 16;         // Max arguments per command
  constexpr size_t INPUT_SANITIZE_MAX_LEN = 256;  // Max input after sanitization
  constexpr size_t TERMINAL_LINE_WIDTH = 40;      // Default terminal output width

  // =========================================================================
  // == Local Gateway Fallback (Offline Mode)
  // =========================================================================
  constexpr uint32_t LOCAL_GATEWAY_FALLBACK_THRESHOLD = 3;          // Switch after N cloud failures
  constexpr unsigned long CLOUD_RETRY_INTERVAL_MS = 5 * 60 * 1000;  // Retry cloud every 5 min
  constexpr uint16_t LOCAL_GATEWAY_PORT = 80;                       // Local gateway HTTP port

  // =========================================================================
  // == Compile-Time Derived Constants (Zero Runtime Cost)
  // =========================================================================

  // Pre-computed values to avoid runtime division/multiplication
  constexpr unsigned long CLOUD_RETRY_INTERVAL_SEC = CLOUD_RETRY_INTERVAL_MS / 1000;
  constexpr unsigned long NTP_RETRY_INTERVAL_SEC = NTP_RETRY_INTERVAL_MS / 1000;
  constexpr unsigned long AUTH_LOCKOUT_DURATION_SEC = REDACTED
  constexpr unsigned long WS_SESSION_TIMEOUT_SEC = WS_SESSION_TIMEOUT_MS / 1000;

  // Buffer size constants (power of 2 for alignment optimization)
  constexpr size_t LOG_BUFFER_SIZE = 256;
  constexpr size_t PATH_BUFFER_SIZE = 64;
  constexpr size_t TAG_BUFFER_SIZE = 16;

  // Hash seeds for compile-time string comparison
  constexpr uint32_t FNV_OFFSET_BASIS = 2166136261u;
  constexpr uint32_t FNV_PRIME = 16777619u;

}  // namespace AppConstants

// =========================================================================
// == Compile-Time Validation (Catch Errors at Build Time)
// =========================================================================

// Validate interval relationships
static_assert(AppConstants::WIFI_CONNECT_TIMEOUT_MS < AppConstants::WIFI_RECONNECT_INTERVAL_MS * 2,
              "REDACTED");

static_assert(AppConstants::NTP_INITIAL_DELAY_MS < AppConstants::NTP_SYNC_TIMEOUT_MS,
              "NTP initial delay should be less than sync timeout");

static_assert(AppConstants::WS_SESSION_CHECK_INTERVAL_MS < AppConstants::WS_SESSION_TIMEOUT_MS,
              "Session check interval must be less than timeout");

// Validate memory thresholds
static_assert(AppConstants::HEAP_CRITICAL_THRESHOLD < AppConstants::HEAP_WARNING_THRESHOLD,
              "Critical threshold must be below warning threshold");

static_assert(AppConstants::API_MIN_SAFE_BLOCK_SIZE >= 1024, "HTTP requires at least 1KB contiguous memory");

// Validate limits
static_assert(AppConstants::MAX_WS_CLIENTS >= 1 && AppConstants::MAX_WS_CLIENTS <= 8,
              "WebSocket clients should be between 1 and 8");

static_assert(AppConstants::MAX_COMMAND_ARGS <= 32, "Command args limit is excessive");

static_assert(AppConstants::FRAGMENTATION_WARNING_PERCENT <= 100, "Fragmentation percentage cannot exceed 100");

// Validate calibration bounds
static_assert(AppConstants::CALIBRATION_OFFSET_MAX > 0, "Calibration offset max must be positive");

static_assert(AppConstants::LUX_FACTOR_MAX >= 1.0f, "Lux factor max should be at least 1.0");

#endif  // CONSTANTS_H
