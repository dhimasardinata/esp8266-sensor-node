#include "LogoutCommand.h"

#include "terminal/DiagnosticsTerminal.h"  // Concrete type for CRTP
#include "support/Utils.h"

LogoutCommand::LogoutCommand(DiagnosticsTerminal& authManager)
    : m_authManager(authManager) {}

void LogoutCommand::execute(const CommandContext& context) {
  uint32_t clientId = REDACTED
  
  if (m_authManager.isClientAuthenticated(clientId)) {
    m_authManager.setClientAuthenticated(clientId, false);
    Utils::ws_printf_P(context.client, PSTR("You have been logged out."));
  } else {
    Utils::ws_printf_P(context.client, PSTR("You were not logged in."));
  }
}
