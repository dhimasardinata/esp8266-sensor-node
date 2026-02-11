#ifndef TERMINAL_FORMATTING_H
#define TERMINAL_FORMATTING_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

/**
 * @brief Terminal formatting utilities for consistent command output.
 * 
 * This namespace provides DRY helper functions for formatting terminal output
 * across all commands, ensuring consistency and reducing code duplication.
 */
namespace TerminalFormat {

  // =========================================================================
  // Time Formatting
  // =========================================================================

  /**
   * @brief Format milliseconds as human-readable uptime (e.g., "2d 5h 30m").
   * @param buffer Output buffer
   * @param len Buffer size
   * @param ms Milliseconds to format
   */
  void formatUptime(char* buffer, size_t len, unsigned long ms);

  /**
   * @brief Format time elapsed since a timestamp (e.g., "5m ago" or "Never").
   * @param buffer Output buffer
   * @param len Buffer size
   * @param timestamp_ms Timestamp in millis() units
   */
  void formatTimeSince(char* buffer, size_t len, unsigned long timestamp_ms);

  // =========================================================================
  // Terminal Output Styling
  // =========================================================================

  /**
   * @brief Print a styled header with double-line borders.
   * @param client WebSocket client
   * @param title Header title text
   * @param emoji Optional emoji prefix (e.g., "📡")
   */
  void printHeader(AsyncWebSocketClient* client, const char* title, const char* emoji = nullptr);

  /**
   * @brief Print a section divider with single-line style.
   * @param client WebSocket client
   * @param title Section title (optional)
   */
  void printSection(AsyncWebSocketClient* client, const char* title = nullptr);

  /**
   * @brief Print a simple horizontal divider.
   * @param client WebSocket client
   * @param style Character to use for divider ('-', '=', etc.)
   * @param width Width of divider (0 = use default)
   */
  void printDivider(AsyncWebSocketClient* client, char style = '-', size_t width = 0);

  // =========================================================================
  // Table Formatting
  // =========================================================================

  /**
   * @brief Print a formatted key-value row.
   * @param client WebSocket client
   * @param label Left-aligned label
   * @param value Right-aligned value
   */
  void printRow(AsyncWebSocketClient* client, const char* label, const char* value);

  /**
   * @brief Print a status row with OK/FAIL indicator.
   * @param client WebSocket client
   * @param label Status item label
   * @param isOk Status value (true = ✓ OK, false = ✗ FAIL)
   */
  void printStatusRow(AsyncWebSocketClient* client, const char* label, bool isOk);

  /**
   * @brief Print a numbered list item.
   * @param client WebSocket client
   * @param index Item number (1-based)
   * @param text Item text
   * @param suffix Optional suffix (e.g., "[Primary]")
   * @param available Optional availability indicator
   */
  void printListItem(AsyncWebSocketClient* client, 
                     size_t index, 
                     const char* text,
                     const char* suffix = nullptr,
                     bool available = true);

  // =========================================================================
  // Message Types
  // =========================================================================

  /**
   * @brief Print an error message with consistent formatting.
   * @param client WebSocket client
   * @param message Error message text
   */
  void printError(AsyncWebSocketClient* client, const char* message);

  /**
   * @brief Print a success message with consistent formatting.
   * @param client WebSocket client
   * @param message Success message text
   */
  void printSuccess(AsyncWebSocketClient* client, const char* message);

  /**
   * @brief Print an info/hint message.
   * @param client WebSocket client
   * @param message Info message text
   */
  void printInfo(AsyncWebSocketClient* client, const char* message);

}  // namespace TerminalFormat

#endif  // TERMINAL_FORMATTING_H
