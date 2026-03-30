#include "FactoryResetCommand.h"

#include "REDACTED"
#include "system/ConfigManager.h"
#include "storage/CacheManager.h"  // Concrete type for CRTP
#include "support/Utils.h"

FactoryResetCommand::FactoryResetCommand(ConfigManager& configManager, CacheManager& cacheManager)
    : m_configManager(configManager), m_cacheManager(cacheManager) {}

void FactoryResetCommand::execute(const CommandContext& context) {
  if (strcmp_P(context.args, PSTR("yes")) != 0 &&
      strcmp_P(context.args, PSTR("confirm")) != 0) {
    Utils::ws_printf_P(context.client, PSTR("[WARNING] This will WIPE ALL DATA. To proceed, type: factoryreset yes"));
    return;
  }
  Utils::ws_printf_P(context.client, PSTR("Performing factory reset... This may take a moment."));
  delay(100);

  // Clear Cache RAM to prevent writes during format
  m_cacheManager.reset();

  if (m_configManager.factoryReset()) {
    // Clear the crash counter in RTC memory to ensure a totally fresh start
    BootGuard::clear();

    Utils::ws_printf_P(context.client, PSTR("SUCCESS: Filesystem formatted. Rebooting now."));
    delay(1000);
    ESP.restart();
  } else {
    Utils::ws_printf_P(context.client,
                       PSTR("FATAL ERROR: Filesystem format FAILED. Please reboot manually and try 'format-fs' command."));
  }
}
