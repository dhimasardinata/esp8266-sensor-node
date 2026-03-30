#include "UplinkCommand.h"

#include <strings.h>

#include "api/ApiClient.h"
#include "system/ConfigManager.h"
#include "support/Utils.h"

namespace {
const char* skipSpaces(const char* s) {
  while (s && *s == ' ') {
    ++s;
  }
  return s ? s : "";
}
}  // namespace

void UplinkCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) {
    return;
  }

  char modeArg[16] = {0};
  const char* args = context.args ? skipSpaces(context.args) : "";
  if (*args != '\0') {
    size_t len = 0;
    while (*args != '\0' && *args != ' ' && len + 1 < sizeof(modeArg)) {
      modeArg[len++] = *args++;
    }
    modeArg[len] = '\0';
  }

  if (modeArg[0] == '\0' || strcasecmp_P(modeArg, PSTR("show")) == 0) {
    char currentMode[8];
    char activeRoute[8];
    m_apiClient.copyUplinkModeString(currentMode, sizeof(currentMode));
    m_apiClient.copyActiveCloudRouteString(activeRoute, sizeof(activeRoute));

    Utils::ws_printf_P(context.client, PSTR("Uplink Mode: %s\n"), currentMode);
    Utils::ws_printf_P(context.client, PSTR("Active Cloud Route: %s\n"), activeRoute);
    Utils::ws_printf_P(context.client, PSTR("Direct Upload URL: %s\n"), m_configManager.getDataUploadUrl());
    Utils::ws_printf_P(context.client, PSTR("Relay Upload URL: %s\n"), DEFAULT_RELAY_DATA_URL);
    m_configManager.releaseStrings();
    return;
  }

  UplinkMode mode = UplinkMode::AUTO;
  if (strcasecmp_P(modeArg, PSTR("auto")) == 0) {
    mode = UplinkMode::AUTO;
  } else if (strcasecmp_P(modeArg, PSTR("direct")) == 0) {
    mode = UplinkMode::DIRECT;
  } else if (strcasecmp_P(modeArg, PSTR("relay")) == 0) {
    mode = UplinkMode::RELAY;
  } else {
    Utils::ws_printf_P(context.client, PSTR("Invalid uplink. Use: show, auto, direct, or relay\n"));
    return;
  }

  m_configManager.setUplinkMode(mode);
  if (m_configManager.save() != ConfigStatus::OK) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Failed to save uplink mode.\n"));
    return;
  }

  switch (mode) {
    case UplinkMode::DIRECT:
      Utils::ws_printf_P(context.client, PSTR("Uplink set to DIRECT (origin API only)\n"));
      break;
    case UplinkMode::RELAY:
      Utils::ws_printf_P(context.client, PSTR("Uplink set to RELAY (proxy only)\n"));
      break;
    case UplinkMode::AUTO:
    default:
      Utils::ws_printf_P(context.client, PSTR("Uplink set to AUTO (direct with relay fallback)\n"));
      break;
  }
}
