#include "FactoryResetCommand.h"

#include "BootGuard.h"
#include "ConfigManager.h"
#include "ICacheManager.h"
#include "utils.h"

FactoryResetCommand::FactoryResetCommand(ConfigManager& configManager, ICacheManager& cacheManager)
    : m_configManager(configManager), m_cacheManager(cacheManager) {}

void FactoryResetCommand::execute(const CommandContext& context) {
  if (strcmp(context.args, "yes") != 0 && strcmp(context.args, "confirm") != 0) {
    Utils::ws_printf(context.client, "[WARNING] This will WIPE ALL DATA. To proceed, type: factoryreset yes");
    return;
  }
  Utils::ws_printf(context.client, "Performing factory reset... This may take a moment.");
  delay(100);

  // Clear Cache RAM to prevent writes during format
  m_cacheManager.reset();

  if (m_configManager.factoryReset()) {
    // Clear the crash counter in RTC memory to ensure a totally fresh start
    BootGuard::clear();

    Utils::ws_printf(context.client, "SUCCESS: Filesystem formatted. Rebooting now.");
    delay(1000);
    ESP.restart();
  } else {
    Utils::ws_printf(context.client,
                     "FATAL ERROR: Filesystem format FAILED. Please reboot manually and try "
                     "'format-fs' command.");
  }
}