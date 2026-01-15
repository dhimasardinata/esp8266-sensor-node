#include "HelpCommand.h"

#include <stdarg.h>
#include <vector>

#include "utils.h"

namespace {
  class BufferedPrinter {
  public:
    BufferedPrinter(AsyncWebSocketClient* client) : m_client(client), m_offset(0) {
      memset(m_buffer, 0, sizeof(m_buffer));
    }

    void append(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
      va_list args;
      va_start(args, fmt);
      int needed = vsnprintf(nullptr, 0, fmt, args);
      va_end(args);
      if (needed < 0) return;

      if (m_offset + needed >= sizeof(m_buffer)) {
        flush();
      }

      va_start(args, fmt);
      m_offset += vsnprintf(m_buffer + m_offset, sizeof(m_buffer) - m_offset, fmt, args);
      va_end(args);
    }

    void flush() {
      if (m_offset > 0) {
        Utils::ws_send_encrypted(m_client, m_buffer);
        m_offset = 0;
        // Secure zeroing
        volatile char* p = m_buffer;
        size_t n = sizeof(m_buffer);
        while(n--) *p++ = 0;
      }
    }

  private:
    AsyncWebSocketClient* m_client;
    char m_buffer[512];
    size_t m_offset;
  };
}

HelpCommand::HelpCommand(const std::vector<std::unique_ptr<ICommand>>& commands) : m_commands(commands) {}

void HelpCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  BufferedPrinter printer(context.client);
  const char* fmtCmd = "  %-18s - %s\n";

  printer.append("\n--- Available Commands ---\n\n[Public]\n");
  for (const auto& cmd : m_commands) {
    if (!cmd->requiresAuth()) printer.append(fmtCmd, cmd->getName(), cmd->getDescription());
  }

  if (context.isAuthenticated) {
    printer.append("\n[Admin]\n");
    for (const auto& cmd : m_commands) {
      if (cmd->requiresAuth()) printer.append(fmtCmd, cmd->getName(), cmd->getDescription());
    }
  } else {
    printer.append("\n[LOCKED] Type 'login <password>' to see Admin Commands.\n");
  }

  printer.append("----------------------\n");
  printer.flush();
}