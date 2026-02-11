#include "FsStatusCommand.h"

#include <LittleFS.h>

#include "utils.h"

namespace {
  void printFsInfo(AsyncWebSocketClient* client, const FSInfo& info) {
    float used_kb = info.usedBytes / 1024.0f;
    float total_kb = REDACTED
    float pct = (total_kb > 0) ? (used_kb / total_kb) * 100.0f : 0;
    Utils::ws_printf(client,
      "\n--- Filesystem Status (LittleFS) ---\n"
      "Total: REDACTED
      "-------------------------------------\n",
      (unsigned)info.totalBytes, (unsigned)info.usedBytes, used_kb, total_kb, pct);
  }
}

void FsStatusCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  FSInfo fs_info;
  if (LittleFS.info(fs_info)) {
    printFsInfo(context.client, fs_info);
  } else {
    Utils::ws_printf(context.client, "\n[ERROR] Failed to get filesystem info.\n");
  }
}