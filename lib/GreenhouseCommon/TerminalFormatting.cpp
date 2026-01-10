#include "TerminalFormatting.h"

#include "constants.h"
#include "utils.h"

namespace TerminalFormat {

  // =========================================================================
  // Time Formatting
  // =========================================================================

  void formatUptime(char* buffer, size_t len, unsigned long ms) {
    if (!buffer || len == 0) return;
    
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;

    if (days > 0) {
      snprintf(buffer, len, "%lud %luh %lum", days, hours, minutes);
    } else {
      snprintf(buffer, len, "%luh %lum %lus", hours, minutes, seconds);
    }
  }

  void formatTimeSince(char* buffer, size_t len, unsigned long timestamp_ms) {
    if (!buffer || len == 0) return;
    
    if (timestamp_ms == 0) {
      snprintf(buffer, len, "Never");
      return;
    }
    formatUptime(buffer, len, millis() - timestamp_ms);
  }

  // =========================================================================
  // Terminal Output Styling
  // =========================================================================

  void printHeader(AsyncWebSocketClient* client, const char* title, const char* emoji) {
    if (!client) return;
    
    constexpr size_t width = AppConstants::TERMINAL_LINE_WIDTH;
    char line[width + 1];
    memset(line, '=', width);
    line[width] = '\0';
    
    Utils::ws_printf(client, "\n%s\n", line);
    if (emoji) {
      Utils::ws_printf(client, "%s %s\n", emoji, title);
    } else {
      Utils::ws_printf(client, "%s\n", title);
    }
    Utils::ws_printf(client, "%s\n\n", line);
  }

  void printSection(AsyncWebSocketClient* client, const char* title) {
    if (!client) return;
    
    if (title) {
      Utils::ws_printf(client, "\n--- %s ---\n", title);
    } else {
      printDivider(client, '-');
    }
  }

  void printDivider(AsyncWebSocketClient* client, char style, size_t width) {
    if (!client) return;
    
    if (width == 0) {
      width = AppConstants::TERMINAL_LINE_WIDTH;
    }
    
    // Use stack buffer for small widths
    if (width <= 64) {
      char line[65];
      memset(line, style, width);
      line[width] = '\0';
      Utils::ws_printf(client, "%s\n", line);
    } else {
      // For larger widths, just print a reasonable amount
      constexpr char longLine[] = "----------------------------------------";
      Utils::ws_printf(client, "%s\n", longLine);
    }
  }

  // =========================================================================
  // Table Formatting
  // =========================================================================

  void printRow(AsyncWebSocketClient* client, const char* label, const char* value) {
    if (!client) return;
    Utils::ws_printf(client, "  %-16s: %s\n", label, value ? value : "-");
  }

  void printStatusRow(AsyncWebSocketClient* client, const char* label, bool isOk) {
    if (!client) return;
    Utils::ws_printf(client, "  %-16s: %s\n", label, isOk ? "OK" : "FAIL");
  }

  void printListItem(AsyncWebSocketClient* client, 
                     size_t index, 
                     const char* text,
                     const char* suffix,
                     bool available) {
    if (!client) return;
    
    if (suffix) {
      Utils::ws_printf(client, "  %zu. %s %s %s\n", 
                       index, 
                       text, 
                       suffix,
                       available ? "Available" : "Not found");
    } else {
      Utils::ws_printf(client, "  %zu. %s %s\n", 
                       index, 
                       text,
                       available ? "Available" : "Not found");
    }
  }

  // =========================================================================
  // Message Types
  // =========================================================================

  void printError(AsyncWebSocketClient* client, const char* message) {
    if (!client) return;
    Utils::ws_printf(client, "[ERROR] %s\n", message);
  }

  void printSuccess(AsyncWebSocketClient* client, const char* message) {
    if (!client) return;
    Utils::ws_printf(client, "[OK] %s\n", message);
  }

  void printInfo(AsyncWebSocketClient* client, const char* message) {
    if (!client) return;
    Utils::ws_printf(client, "[INFO] %s\n", message);
  }

}  // namespace TerminalFormat
