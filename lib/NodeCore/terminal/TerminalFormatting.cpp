#include "terminal/TerminalFormatting.h"

#include "config/constants.h"
#include "support/Utils.h"

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

    void append_literal_P(char* buffer, size_t len, size_t& pos, PGM_P text) {
      if (!buffer || pos >= len || !text)
        return;
      size_t remaining = len - pos;
      if (remaining <= 1)
        return;
      size_t n = strlen_P(text);
      if (n >= remaining)
        n = remaining - 1;
      memcpy_P(buffer + pos, text, n);
      pos += n;
      buffer[pos] = '\0';
    }

    void copy_literal_P(char* out, size_t out_len, PGM_P text) {
      if (!out || out_len == 0) {
        return;
      }
      if (!text) {
        out[0] = '\0';
        return;
      }
      strncpy_P(out, text, out_len - 1);
      out[out_len - 1] = '\0';
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
      append_literal_P(buffer, len, pos, PSTR("d "));
      append_u32(buffer, len, pos, hours);
      append_literal_P(buffer, len, pos, PSTR("h "));
      append_u32(buffer, len, pos, minutes);
      append_literal_P(buffer, len, pos, PSTR("m"));
    } else {
      append_u32(buffer, len, pos, hours);
      append_literal_P(buffer, len, pos, PSTR("h "));
      append_u32(buffer, len, pos, minutes);
      append_literal_P(buffer, len, pos, PSTR("m "));
      append_u32(buffer, len, pos, seconds);
      append_literal_P(buffer, len, pos, PSTR("s"));
    }
  }

  void formatTimeSince(char* buffer, size_t len, unsigned long timestamp_ms) {
    if (!buffer || len == 0) return;
    
    if (timestamp_ms == 0) {
      size_t pos = 0;
      buffer[0] = '\0';
      append_literal_P(buffer, len, pos, PSTR("Never"));
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
    
    Utils::ws_printf_P(client, PSTR("\n%s\n"), line);
    if (emoji) {
      Utils::ws_printf_P(client, PSTR("%s %s\n"), emoji, title);
    } else {
      Utils::ws_printf_P(client, PSTR("%s\n"), title);
    }
    Utils::ws_printf_P(client, PSTR("%s\n\n"), line);
  }

  void printSection(AsyncWebSocketClient* client, const char* title) {
    if (!client) return;
    
    if (title) {
      Utils::ws_printf_P(client, PSTR("\n--- %s ---\n"), title);
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
      Utils::ws_printf_P(client, PSTR("%s\n"), line);
    } else {
      // For larger widths, just print a reasonable amount
      constexpr char longLine[] = "----------------------------------------";
      Utils::ws_printf_P(client, PSTR("%s\n"), longLine);
    }
  }

  // =========================================================================
  // Table Formatting
  // =========================================================================

  void printRow(AsyncWebSocketClient* client, const char* label, const char* value) {
    if (!client) return;
    char valueBuf[32];
    copy_literal_P(valueBuf, sizeof(valueBuf), PSTR("-"));
    Utils::ws_printf_P(client, PSTR("  %-16s: %s\n"), label, value ? value : valueBuf);
  }

  void printRow(AsyncWebSocketClient* client, const char* label, const __FlashStringHelper* value) {
    if (!client) return;
    char valueBuf[32];
    copy_literal_P(valueBuf, sizeof(valueBuf), value ? reinterpret_cast<PGM_P>(value) : PSTR("-"));
    Utils::ws_printf_P(client, PSTR("  %-16s: %s\n"), label, valueBuf);
  }

  void printStatusRow(AsyncWebSocketClient* client, const char* label, bool isOk) {
    if (!client) return;
    char status[8];
    copy_literal_P(status, sizeof(status), isOk ? PSTR("OK") : PSTR("FAIL"));
    Utils::ws_printf_P(client, PSTR("  %-16s: %s\n"), label, status);
  }

  void printListItem(AsyncWebSocketClient* client, 
                     size_t index, 
                     const char* text,
                     const char* suffix,
                     bool available) {
    if (!client) return;
    
    char availability[12];
    copy_literal_P(availability, sizeof(availability), available ? PSTR("Available") : PSTR("Not found"));
    if (suffix) {
      Utils::ws_printf_P(client, PSTR("  %zu. %s %s %s\n"),
                       index,
                       text,
                       suffix,
                       availability);
    } else {
      Utils::ws_printf_P(client, PSTR("  %zu. %s %s\n"),
                       index,
                       text,
                       availability);
    }
  }

  // =========================================================================
  // Message Types
  // =========================================================================

  void printError(AsyncWebSocketClient* client, const char* message) {
    if (!client) return;
    Utils::ws_printf_P(client, PSTR("[ERROR] %s\n"), message);
  }

  void printSuccess(AsyncWebSocketClient* client, const char* message) {
    if (!client) return;
    Utils::ws_printf_P(client, PSTR("[OK] %s\n"), message);
  }

  void printInfo(AsyncWebSocketClient* client, const char* message) {
    if (!client) return;
    Utils::ws_printf_P(client, PSTR("[INFO] %s\n"), message);
  }

}  // namespace TerminalFormat
