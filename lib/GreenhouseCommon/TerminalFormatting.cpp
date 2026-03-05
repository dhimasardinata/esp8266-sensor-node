#include "TerminalFormatting.h"

#include "constants.h"
#include "utils.h"

namespace TerminalFormat {

  // =========================================================================
  // Time Formatting
  // =========================================================================

  namespace {
    size_t u32_to_dec(char* out, size_t out_len, unsigned long value) {
      if (!out || out_len == 0)
        return 0;
      char tmp[10];
      size_t n = 0;
      do {
        tmp[n++] = static_cast<char>('0' + (value % 10));
        value /= 10;
      } while (value != 0 && n < sizeof(tmp));
      size_t written = 0;
      while (n > 0 && written + 1 < out_len) {
        out[written++] = tmp[--n];
      }
      out[written] = '\0';
      return written;
    }

    void append_literal(char* buffer, size_t len, size_t& pos, const char* text) {
      if (!buffer || pos >= len || !text)
        return;
      size_t remaining = len - pos;
      if (remaining <= 1)
        return;
      size_t n = strnlen(text, remaining - 1);
      memcpy(buffer + pos, text, n);
      pos += n;
      buffer[pos] = '\0';
    }

    void append_u32(char* buffer, size_t len, size_t& pos, unsigned long value) {
      if (!buffer || pos >= len)
        return;
      size_t remaining = len - pos;
      if (remaining <= 1)
        return;
      size_t n = u32_to_dec(buffer + pos, remaining, value);
      pos += n;
      if (pos < len)
        buffer[pos] = '\0';
    }
  }  // namespace

  void formatUptime(char* buffer, size_t len, unsigned long ms) {
    if (!buffer || len == 0) return;
    
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;

    buffer[0] = '\0';
    size_t pos = 0;
    if (days > 0) {
      append_u32(buffer, len, pos, days);
      append_literal(buffer, len, pos, "d ");
      append_u32(buffer, len, pos, hours);
      append_literal(buffer, len, pos, "h ");
      append_u32(buffer, len, pos, minutes);
      append_literal(buffer, len, pos, "m");
    } else {
      append_u32(buffer, len, pos, hours);
      append_literal(buffer, len, pos, "h ");
      append_u32(buffer, len, pos, minutes);
      append_literal(buffer, len, pos, "m ");
      append_u32(buffer, len, pos, seconds);
      append_literal(buffer, len, pos, "s");
    }
  }

  void formatTimeSince(char* buffer, size_t len, unsigned long timestamp_ms) {
    if (!buffer || len == 0) return;
    
    if (timestamp_ms == 0) {
      size_t pos = 0;
      buffer[0] = '\0';
      append_literal(buffer, len, pos, "Never");
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
