#include "FsStatusCommand.h"

#include <LittleFS.h>

#include "support/Utils.h"

namespace {
  void printFsInfo(AsyncWebSocketClient* client, const FSInfo& info) {
    float used_kb = info.usedBytes / 1024.0f;
    float total_kb = REDACTED
    float pct = (total_kb > 0) ? (used_kb / total_kb) * 100.0f : 0;
    Utils::ws_printf_P(client, PSTR("\n--- Filesystem Status (LittleFS) ---\n"));
    Utils::ws_printf_P(client, PSTR("Total: %u bytes | Used: %u bytes\n"), (unsigned)info.totalBytes, (unsigned)info.usedBytes);
    Utils::ws_printf_P(client, PSTR("Usage: %.2f KB / %.2f KB (%.1f%%)\n"), used_kb, total_kb, pct);
    Utils::ws_printf_P(client, PSTR("-------------------------------------\n"));
  }
}

void FsStatusCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  FSInfo fs_info;
  if (LittleFS.info(fs_info)) {
    printFsInfo(context.client, fs_info);
  } else {
    Utils::ws_printf_P(context.client, PSTR("\n[ERROR] Failed to get filesystem info.\n"));
  }
}
