#include "LogoutCommand.h"

#include "IAuthManager.h"
#include "utils.h"

LogoutCommand::LogoutCommand(IAuthManager& authManager)
    : m_authManager(authManager) {}

void LogoutCommand::execute(const CommandContext& context) {
  uint32_t clientId = context.client->id();
  
  if (m_authManager.isClientAuthenticated(clientId)) {
    m_authManager.setClientAuthenticated(clientId, false);
    Utils::ws_printf(context.client, "You have been logged out.");
  } else {
    Utils::ws_printf(context.client, "You were not logged in.");
  }
}