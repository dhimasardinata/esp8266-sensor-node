#include "FormatFsCommand.h"

#include <Arduino.h>
#include <LittleFS.h>

#include "system/ConfigManager.h"
#include "support/Utils.h"

FormatFsCommand::FormatFsCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void FormatFsCommand::execute(const CommandContext& context) {
  if (strcmp_P(context.args, PSTR("yes")) != 0 &&
      strcmp_P(context.args, PSTR("confirm")) != 0) {
    Utils::ws_printf_P(context.client, PSTR("[WARNING] This will FORMAT THE FILESYSTEM. To proceed, type: format-fs yes"));
    return;
  }
  Utils::ws_printf_P(context.client, PSTR("Formatting LittleFS... This may take a moment."));
  ESP.wdtDisable();
  bool success = LittleFS.format();
  ESP.wdtEnable(8000);
  if (success) {
    Utils::ws_printf_P(context.client, PSTR("Filesystem formatted. Please reboot the device."));
  } else {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Filesystem format failed."));
  }
}
