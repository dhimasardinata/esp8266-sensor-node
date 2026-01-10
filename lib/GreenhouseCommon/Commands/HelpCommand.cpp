#include "HelpCommand.h"

#include <stdarg.h>

#include <vector>

#include "utils.h"

HelpCommand::HelpCommand(const std::vector<std::unique_ptr<ICommand>>& commands) : m_commands(commands) {}

void HelpCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) {
    return;
  }

  // Buffer di stack, ukuran aman untuk RAM ESP8266
  char buffer[512];
  size_t offset = 0;

  // Format kolom rapi
  const char* formatCmd = "  %-18s - %s\n";

  // --- HELPER LAMBDA: SMART APPEND ---
  // Fungsi ini otomatis menghitung ukuran string.
  // Jika tidak muat di buffer, dia kirim dulu (flush), baru tulis.
  // Semuanya berbasis sizeof(buffer).
  auto append = [&](const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    va_list args;

    // 1. Hitung ukuran yang DIBUTUHKAN (Dry Run)
    va_start(args, fmt);
    int needed = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    if (needed < 0)
      return;  // Error encoding

    // 2. Cek apakah muat di sisa buffer?
    // sizeof(buffer) otomatis mengambil ukuran 512
    if (offset + needed >= sizeof(buffer)) {
      // Tidak muat -> Kirim yang ada sekarang
      Utils::ws_send_encrypted(context.client, buffer);

      // Reset buffer
      offset = 0;
      memset(buffer, 0, sizeof(buffer));
    }

    // 3. Tulis data (Dijamin muat atau buffer baru)
    va_start(args, fmt);
    // sizeof(buffer) - offset memastikan kita tidak pernah overflow
    offset += vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);
    va_end(args);
  };

  // --- MENGISI KONTEN ---

  append("\n--- Available Commands ---\n");

  // Public Commands
  append("\n[Public]\n");
  for (const auto& cmd_ptr : m_commands) {
    if (!cmd_ptr->requiresAuth()) {
      append(formatCmd, cmd_ptr->getName(), cmd_ptr->getDescription());
    }
  }

  // Admin Commands
  if (context.isAuthenticated) {
    append("\n[Admin]\n");
    for (const auto& cmd_ptr : m_commands) {
      if (cmd_ptr->requiresAuth()) {
        append(formatCmd, cmd_ptr->getName(), cmd_ptr->getDescription());
      }
    }
  } else {
    append("\n[LOCKED] Type 'login <password>' to see Admin Commands.\n");
  }

  append("----------------------\n");

  // Kirim sisa buffer terakhir (jika ada)
  if (offset > 0) {
    Utils::ws_send_encrypted(context.client, buffer);
  }
}