#include "REDACTED"

#include "system/ConfigManager.h"
#include "support/Utils.h"

SetWifiCommand:REDACTED

namespace {
  bool validateCredentials(const char* ssid, const char* pass, AsyncWebSocketClient* client) {
    size_t ssidLen = REDACTED
    size_t passLen = REDACTED
    if (ssidLen =REDACTED
      Utils::ws_printf_P(client, PSTR("[ERROR] SSID invalid or too long."));
      return false;
    }
    if (passLen >= REDACTED
      Utils::ws_printf_P(client, PSTR("[ERROR] Password too long."));
      return false;
    }
    if (!Utils::isSafeString(std::string_view(ssid, ssidLen)) ||
        !Utils::isSafeString(std::string_view(pass, passLen))) {
      Utils::ws_printf_P(client, PSTR("[ERROR] Invalid characters."));
      return false;
    }
    return true;
  }
}

void SetWifiCommand:REDACTED
  const char* argv[5];
  char buffer[256];
  size_t argLen = strnlen(context.args, sizeof(buffer) - 1);
  memcpy(buffer, context.args, argLen);
  buffer[argLen] = '\0';

  size_t argc = Utils::tokenize_quoted_args(buffer, argv, 5);
  if (argc < 1 || argc > 2) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Usage: setwifi \"SSID\" \"PASS\""));
    return;
  }

  const char* ssid = REDACTED
  const char* password = REDACTED

  if (!validateCredentials(ssid, password, context.client)) return;

  if (ConfigManager::saveTempWifiCredentials(ssid, password)) {
    Utils::ws_printf_P(context.client, PSTR("WiFi saved. Rebooting..."));
    delay(3000);
    ESP.restart();
  } else {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Failed to save credentials."));
  }
}
