#include "FormatFsCommand.h"

#include <Arduino.h>
#include <LittleFS.h>

#include "ConfigManager.h"
#include "utils.h"

FormatFsCommand::FormatFsCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void FormatFsCommand::execute(const CommandContext& context) {
  if (strcmp(context.args, "yes") != 0 && strcmp(context.args, "confirm") != 0) {
    Utils::ws_printf(context.client, "[WARNING] This will FORMAT THE FILESYSTEM. To proceed, type: format-fs yes");
    return;
  }
  Utils::ws_printf(context.client, "Formatting LittleFS... This may take a moment.");
  ESP.wdtDisable();
  bool success = LittleFS.format();
  ESP.wdtEnable(8000);
  if (success) {
    Utils::ws_printf(context.client, "Filesystem formatted. Please reboot the device.");
  } else {
    Utils::ws_printf(context.client, "[ERROR] Filesystem format failed.");
  }
}
