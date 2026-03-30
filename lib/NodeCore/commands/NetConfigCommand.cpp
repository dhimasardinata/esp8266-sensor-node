#include "NetConfigCommand.h"

#include <cstring>
#include <strings.h>

#include "api/ApiClient.h"
#include "system/ConfigManager.h"
#include "support/Utils.h"

#ifndef FACTORY_API_TOKEN
#define FACTORY_API_TOKEN "REDACTED"
#endif

#ifndef FACTORY_OTA_TOKEN
#define FACTORY_OTA_TOKEN "REDACTED"
#endif

namespace {
  const char* skipSpaces(const char* s) {
    while (s && *s == ' ') {
      ++s;
    }
    return s ? s : "";
  }

  const char* getUploadTokenState(ConfigManager& configManager) {
    const char* uploadToken = REDACTED
    const bool uploadEmpty = (!uploadToken || uploadToken[0] == '\0');
    const bool uploadFactory = (!uploadEmpty && strcmp(uploadToken, FACTORY_API_TOKEN) == 0);
    return uploadEmpty ? "empty" : (uploadFactory ? "factory-default" : "set");
  }

  const char* getOtaTokenState(ConfigManager& configManager) {
    return configManager.hasOtaTokenOverride() ? "override-set" : REDACTED
  }

  const char* getEffectiveUploadSource(ConfigManager& configManager) {
    const char* uploadToken = REDACTED
    if (uploadToken && uploadToken[0] != REDACTED
      return "upload-slot";
    }
    if (configManager.hasOtaTokenOverride()) {
      return "REDACTED";
    }
    if (FACTORY_OTA_TOKEN[0] != REDACTED
      return "REDACTED";
    }
    return "none";
  }

  const char* getEffectiveOtaSource(ConfigManager& configManager) {
    if (configManager.hasOtaTokenOverride()) {
      return "REDACTED";
    }
    if (FACTORY_OTA_TOKEN[0] != REDACTED
      return "REDACTED";
    }
    const char* uploadToken = REDACTED
    if (uploadToken && uploadToken[0] != REDACTED
      return "upload-slot";
    }
    return "none";
  }
}  // namespace

NetConfigCommand::NetConfigCommand(ConfigManager& configManager, ApiClient& apiClient)
    : m_configManager(configManager), m_apiClient(apiClient) {}

void NetConfigCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) {
    return;
  }

  const char* args = context.args ? skipSpaces(context.args) : "";
  if (*args != '\0' && strcasecmp(args, "show") != 0) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Usage: netconfig [show]\n"));
    return;
  }

  const char* uploadUrl = m_configManager.getDataUploadUrl();
  const char* otaUrl = REDACTED
  const char* gh1Host = m_configManager.getGatewayHost(1);
  const char* gh1Ip = m_configManager.getGatewayIp(1);
  const char* gh2Host = m_configManager.getGatewayHost(2);
  const char* gh2Ip = m_configManager.getGatewayIp(2);

  const char* uploadTokenState = REDACTED
  const char* otaTokenState = REDACTED
  const char* uploadTokenSource = REDACTED
  const char* otaTokenSource = REDACTED

  char mode[8];
  char uplink[8];
  char activeRoute[8];
  char gatewayState[4];
  m_apiClient.copyUploadModeString(mode, sizeof(mode));
  m_apiClient.copyUplinkModeString(uplink, sizeof(uplink));
  m_apiClient.copyActiveCloudRouteString(activeRoute, sizeof(activeRoute));
  strncpy_P(gatewayState,
            m_apiClient.isLocalGatewayActive() ? PSTR("yes") : PSTR("no"),
            sizeof(gatewayState) - 1);
  gatewayState[sizeof(gatewayState) - 1] = '\0';

  Utils::ws_printf_P(context.client, PSTR("Network Configuration:\n"));
  Utils::ws_printf_P(context.client,
                     PSTR("  Upload URL         : %s\n"),
                     (uploadUrl && uploadUrl[0] != '\0') ? uploadUrl : "<none>");
  Utils::ws_printf_P(context.client,
                     PSTR("  OTA URL            : REDACTED
                     (otaUrl && otaUrl[0] != REDACTED
  Utils::ws_printf_P(context.client, PSTR("  Relay Upload URL   : %s\n"), DEFAULT_RELAY_DATA_URL);
  Utils::ws_printf_P(context.client, PSTR("  Upload Token       : %s\n"), uploadTokenState);
  Utils::ws_printf_P(context.client, PSTR("  OTA Token          : %s\n"), otaTokenState);
  Utils::ws_printf_P(context.client, PSTR("  Upload Token Uses  : %s\n"), uploadTokenSource);
  Utils::ws_printf_P(context.client, PSTR("  OTA Token Uses     : %s\n"), otaTokenSource);
  Utils::ws_printf_P(context.client,
                     PSTR("  Gateway GH1 Host   : %s\n"),
                     (gh1Host && gh1Host[0] != '\0') ? gh1Host : "<none>");
  Utils::ws_printf_P(context.client,
                     PSTR("  Gateway GH1 IP     : %s\n"),
                     (gh1Ip && gh1Ip[0] != '\0') ? gh1Ip : "<none>");
  Utils::ws_printf_P(context.client,
                     PSTR("  Gateway GH2 Host   : %s\n"),
                     (gh2Host && gh2Host[0] != '\0') ? gh2Host : "<none>");
  Utils::ws_printf_P(context.client,
                     PSTR("  Gateway GH2 IP     : %s\n"),
                     (gh2Ip && gh2Ip[0] != '\0') ? gh2Ip : "<none>");
  Utils::ws_printf_P(context.client, PSTR("  Upload Mode        : %s\n"), mode);
  Utils::ws_printf_P(context.client, PSTR("  Uplink Mode        : %s\n"), uplink);
  Utils::ws_printf_P(context.client, PSTR("  Active Cloud Route : %s\n"), activeRoute);
  Utils::ws_printf_P(context.client, PSTR("  Gateway Active     : %s\n"), gatewayState);

  m_configManager.releaseStrings();
}
