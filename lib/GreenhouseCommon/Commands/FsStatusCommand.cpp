#include "FsStatusCommand.h"

#include <LittleFS.h>

#include "utils.h"

void FsStatusCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) {
    return;
  }

  FSInfo fs_info;
  if (LittleFS.info(fs_info)) {
    float used_kb = fs_info.usedBytes / 1024.0f;
    float total_kb = fs_info.totalBytes / 1024.0f;
    float percentage = (total_kb > 0) ? (used_kb / total_kb) * 100.0f : 0;

    // Hapus <384>
    Utils::ws_printf(context.client,
                     "\n--- Filesystem Status (LittleFS) ---\n"
                     "Total Bytes     : %u bytes\n"
                     "Used Bytes      : %u bytes\n"
                     "\nUsage           : %.2f KB / %.2f KB (%.1f%%)\n"
                     "-------------------------------------\n",
                     (unsigned int)fs_info.totalBytes,
                     (unsigned int)fs_info.usedBytes,
                     used_kb,
                     total_kb,
                     percentage);
  } else {
    Utils::ws_printf(context.client, "\n[ERROR] Failed to get filesystem info.\n");
  }
}