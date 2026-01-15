#include "SetTokenCommand.h"

#include "ConfigManager.h"
#include "utils.h"

SetTokenCommand::SetTokenCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void SetTokenCommand::execute(const CommandContext& context) {
  // Check for empty string
  if (context.args[0] == '\0') {
    Utils::ws_printf(context.client, "[ERROR] Auth Token cannot be empty. Usage: settoken <new_token>");
    return;
  }

  // Warning for unusual format (e.g. missing '|')
  if (strchr(context.args, '|') == nullptr) {
    Utils::ws_printf(context.client, "[WARNING] Token format looks unusual (expected 'ID|SECRET'). Saving anyway...");
  }

  // Pass const char* directly
  m_configManager.setAuthToken(context.args);

  if (m_configManager.save() == ConfigStatus::OK) {
    Utils::ws_printf(context.client, "Auth Token updated and saved.");
  } else {
    Utils::ws_printf(context.client, "[ERROR] Failed to save token.");
  }
}