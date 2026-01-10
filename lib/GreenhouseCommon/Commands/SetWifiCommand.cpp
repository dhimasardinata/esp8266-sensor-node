#include "SetWifiCommand.h"

#include <vector>

#include "ConfigManager.h"
#include "utils.h"

SetWifiCommand::SetWifiCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void SetWifiCommand::execute(const CommandContext& context) {
  // Use a local array of pointers for arguments
  const char* argv[5];

  // We need a mutable copy of args because tokenize modifies it
  // context.args is const char*, so we copy to stack
  char buffer[256];

  // Safe copy with null termination
  strncpy(buffer, context.args, sizeof(buffer));
  buffer[sizeof(buffer) - 1] = '\0';

  size_t argc = Utils::tokenize_quoted_args(buffer, argv, 5);

  if (argc < 1 || argc > 2) {
    Utils::ws_printf(context.client, "[ERROR] Usage: setwifi \"SSID\" \"PASS\"");
    return;
  }

  const char* ssid = argv[0];
  const char* password = (argc == 2) ? argv[1] : "";

  if (strlen(ssid) == 0 || strlen(ssid) >= MAX_WIFI_CRED_LEN) {
    Utils::ws_printf(context.client, "[ERROR] SSID is invalid or too long.");
    return;
  }

  if (strlen(password) >= MAX_WIFI_CRED_LEN) {
    Utils::ws_printf(context.client, "[ERROR] Password is too long.");
    return;
  }

  if (!Utils::isSafeString(ssid) || !Utils::isSafeString(password)) {
    Utils::ws_printf(context.client, "[ERROR] Inputs contain invalid characters. Use printable ASCII only.");
    return;
  }

  if (ConfigManager::saveTempWifiCredentials(ssid, password)) {
    Utils::ws_printf(context.client, "New WiFi credentials saved. Device will reboot in 3 seconds to apply...");
    delay(3000);
    ESP.restart();
  } else {
    Utils::ws_printf(context.client, "[ERROR] Failed to save new WiFi credentials to filesystem.");
  }
}