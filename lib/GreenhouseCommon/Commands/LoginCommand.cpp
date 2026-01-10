#include "LoginCommand.h"

#include "ConfigManager.h"
#include "IAuthManager.h"
#include "utils.h"

LoginCommand::LoginCommand(ConfigManager& configManager, IAuthManager& authManager)
    : m_configManager(configManager), m_authManager(authManager) {}

void LoginCommand::execute(const CommandContext& context) {
  if (!context.client) return;
  uint32_t clientId = context.client->id();

  // Check lockout
  if (m_authManager.isClientLockedOut(clientId)) {
    Utils::ws_printf(context.client, "[ERROR] Too many failed login attempts. Please try again later.");
    return;
  }

  if (strlen(context.args) == 0) {
    Utils::ws_printf(context.client, "[ERROR] Usage: login <password>");
    return;
  }

  char hashed_pass_buffer[65];
  Utils::hash_sha256(hashed_pass_buffer, context.args);

  if (strcmp(hashed_pass_buffer, m_configManager.getConfig().ADMIN_PASSWORD.data()) == 0) {
    m_authManager.setClientAuthenticated(clientId, true);
    m_authManager.clearFailedLogins(clientId);
    Utils::ws_printf(context.client, "Authentication successful.");
  } else {
    m_authManager.recordFailedLogin(clientId);
    Utils::ws_printf(context.client, "[ERROR] Authentication failed. Invalid password.");
  }
}