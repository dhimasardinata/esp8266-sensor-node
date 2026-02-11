/**
 * @file Paths.h
 * @brief Centralized file paths and web routes for the greenhouse node.
 * 
 * This consolidates all magic strings to make maintenance easier
 * and reduce the risk of typos causing runtime issues.
 */
#ifndef PATHS_H
#define PATHS_H

namespace Paths {

  // =========================================================================
  // == Filesystem Paths (LittleFS)
  // =========================================================================
  
  /// Main configuration file
  constexpr const char* CONFIG_FILE = "/config.dat";
  /// Configuration backup (for atomic writes)
  constexpr const char* CONFIG_BACKUP = "/config.bak";
  /// Temporary file during config save
  constexpr const char* CONFIG_TEMP = "/config.tmp";
  
  /// WiFi credentials file
  constexpr const char* WIFI_MAIN = REDACTED
  /// Temporary WiFi credentials (portal testing)
  constexpr const char* WIFI_TEMP = REDACTED
  
  /// WiFi credential store (multi-network)
  constexpr const char* WIFI_LIST = REDACTED
  
  /// Sensor data cache (store-and-forward)
  constexpr const char* CACHE_FILE = "/cache.dat";
  
  /// Crash log for post-mortem analysis
  constexpr const char* CRASH_LOG = "/crash.log";
  
  /// Firmware update file (staged before flash)
  constexpr const char* UPDATE_BIN = "/update.bin";

  // =========================================================================
  // == Web Routes (HTTP Server)
  // =========================================================================
  
  /// Dashboard/index page
  constexpr const char* ROUTE_ROOT = "/";
  /// Terminal CLI interface
  constexpr const char* ROUTE_TERMINAL = "/terminal";
  /// Firmware update page
  constexpr const char* ROUTE_UPDATE = "/update";
  /// Status API endpoint
  constexpr const char* ROUTE_API_STATUS = "/api/status";
  
  // Portal routes
  constexpr const char* ROUTE_PORTAL_SAVE = "/save";
  constexpr const char* ROUTE_PORTAL_CONNECTING = "/connecting";
  constexpr const char* ROUTE_PORTAL_STATUS = "/status";
  constexpr const char* ROUTE_PORTAL_NETWORKS = "/networks";
  constexpr const char* ROUTE_PORTAL_SAVED = "/saved";
  constexpr const char* ROUTE_PORTAL_FORGET = "/forget";
  constexpr const char* ROUTE_PORTAL_SUCCESS = "/success";

  // =========================================================================
  // == WebSocket Paths
  // =========================================================================
  
  /// WebSocket endpoint for terminal
  constexpr const char* WS_TERMINAL = "/ws";

}  // namespace Paths

#endif  // PATHS_H
