#include "LoginCommand.h"

#include "ConfigManager.h"
#include "IAuthManager.h"
#include "utils.h"

LoginCommand::LoginCommand(ConfigManager& configManager, IAuthManager& authManager)
    : m_configManager(configManager), m_authManager(authManager) {}

void LoginCommand::execute(const CommandContext& context) {
  if (!context.client) return;
  uint32_t clientId = context.client->id();

  if (m_authManager.isClientLockedOut(clientId)) {
    Utils::ws_printf(context.client, "[ERROR] Too many attempts. Try later.");
    return;
  }
  if (strnlen(context.args, 1) == 0) {
    Utils::ws_printf(context.client, "[ERROR] Usage: login <password>");
    return;
  }

  char hash[65];
  (void)Utils::hash_sha256(hash, context.args);
  bool valid = (strcmp(hash, m_configManager.getConfig().ADMIN_PASSWORD.data()) == 0);
  
  if (valid) {
    m_authManager.setClientAuthenticated(clientId, true);
    m_authManager.clearFailedLogins(clientId);
    Utils::ws_printf(context.client, "Authentication successful.");
  } else {
    m_authManager.recordFailedLogin(clientId);
    Utils::ws_printf(context.client, "[ERROR] Authentication failed.");
  }
}