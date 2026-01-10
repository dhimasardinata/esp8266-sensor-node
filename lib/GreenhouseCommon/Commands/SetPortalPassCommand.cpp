#include "SetPortalPassCommand.h"

#include "ConfigManager.h"
#include "utils.h"

SetPortalPassCommand::SetPortalPassCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void SetPortalPassCommand::execute(const CommandContext& context) {
  // Use strlen instead of .length()
  if (strlen(context.args) < 8) {
    Utils::ws_printf(context.client, "[ERROR] Portal password must be at least 8 characters.");
    return;
  }

  if (!Utils::isSafeString(context.args)) {
      Utils::ws_printf(context.client, "[ERROR] Password contains invalid characters.");
      return;
  }

  // Pass context.args directly (it is const char*)
  m_configManager.setPortalPassword(context.args);

  if (m_configManager.save() == ConfigStatus::OK) {
    Utils::ws_printf(context.client, "Captive Portal password updated and saved.");
  } else {
    Utils::ws_printf(context.client, "[ERROR] Failed to save new portal password.");
  }
}