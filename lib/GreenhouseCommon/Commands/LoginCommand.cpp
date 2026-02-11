#include "LoginCommand.h"

#include "ConfigManager.h"
#include "DiagnosticsTerminal.h"  // Concrete type for CRTP
#include "utils.h"

LoginCommand::LoginCommand(ConfigManager& configManager, DiagnosticsTerminal& authManager)
    : m_configManager(configManager), m_authManager(authManager) {}

void LoginCommand::execute(const CommandContext& context) {
  if (!context.client) return;
  uint32_t clientId = REDACTED

  if (m_authManager.isClientLockedOut(clientId)) {
    Utils::ws_printf(context.client, "[ERROR] Too many attempts. Try later.");
    return;
  }
  // Trim arguments to handle potential trailing whitespace from terminals
  // Use a small stack buffer (max usage is constrained)
  char argBuffer[64];
  size_t argLen = strnlen(context.args, sizeof(argBuffer) - 1);
  memcpy(argBuffer, context.args, argLen);
  argBuffer[argLen] = '\0';
  
  // Use span for trim_inplace
  std::span<char> argSpan(argBuffer, argLen);
  Utils::trim_inplace(argSpan);
  argLen = strnlen(argBuffer, sizeof(argBuffer) - 1);

  // Check if trimmed string is empty (check content, not span size)
  if (argBuffer[0] == '\0') {
    Utils::ws_printf(context.client, "[ERROR] Usage: login <password>");
    return;
  }

  char hash[65];
  // Calculate hash of trimmed password
  (void)Utils::hash_sha256(hash, std::string_view(argBuffer, argLen));
  
  // Primary Check: Compare against stored Admin Password Hash
  // NOTE: ADMIN_PASSWORD in config is the SHA256 HASH of the actual password.
  bool valid = Utils::consttime_equal(hash, m_configManager.getAdminPassword(), 64);

  // Secondary Check: Compare against Portal Password (Plain Text)
  // This helps if the admin hash is out of sync or if the user changed the portal pass.
  if (!valid) {
    valid = (strncmp(argBuffer, m_configManager.getPortalPassword(), 64) == 0);
  }

  m_configManager.releaseStrings();
  
  if (valid) {
    m_authManager.setClientAuthenticated(clientId, true);
    m_authManager.clearFailedLogins(clientId);
    Utils::ws_printf(context.client, "Authentication successful.");
  } else {
    m_authManager.recordFailedLogin(clientId);
    Utils::ws_printf(context.client, "[ERROR] Authentication failed.");
  }
}
