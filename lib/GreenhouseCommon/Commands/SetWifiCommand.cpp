#include "SetWifiCommand.h"

#include <vector>

#include "ConfigManager.h"
#include "utils.h"

SetWifiCommand::SetWifiCommand(ConfigManager& configManager) : m_configManager(configManager) {}

namespace {
  bool validateCredentials(const char* ssid, const char* pass, AsyncWebSocketClient* client) {
    size_t ssidLen = strnlen(ssid, MAX_WIFI_CRED_LEN);
    size_t passLen = strnlen(pass, MAX_WIFI_CRED_LEN);
    if (ssidLen == 0 || ssidLen >= MAX_WIFI_CRED_LEN) {
      Utils::ws_printf(client, "[ERROR] SSID invalid or too long.");
      return false;
    }
    if (passLen >= MAX_WIFI_CRED_LEN) {
      Utils::ws_printf(client, "[ERROR] Password too long.");
      return false;
    }
    if (!Utils::isSafeString(ssid) || !Utils::isSafeString(pass)) {
      Utils::ws_printf(client, "[ERROR] Invalid characters.");
      return false;
    }
    return true;
  }
}

void SetWifiCommand::execute(const CommandContext& context) {
  const char* argv[5];
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "%s", context.args);  // Safe copy

  size_t argc = Utils::tokenize_quoted_args(buffer, argv, 5);
  if (argc < 1 || argc > 2) {
    Utils::ws_printf(context.client, "[ERROR] Usage: setwifi \"SSID\" \"PASS\"");
    return;
  }

  const char* ssid = argv[0];
  const char* password = (argc == 2) ? argv[1] : "";

  if (!validateCredentials(ssid, password, context.client)) return;

  if (ConfigManager::saveTempWifiCredentials(ssid, password)) {
    Utils::ws_printf(context.client, "WiFi saved. Rebooting...");
    delay(3000);
    ESP.restart();
  } else {
    Utils::ws_printf(context.client, "[ERROR] Failed to save credentials.");
  }
}