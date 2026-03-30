#include "ModeCommand.h"

#include "api/ApiClient.h"
#include "CommandContext.h"
#include "support/Utils.h"

void ModeCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;
  
  char modeArg[16] = {0};
  
  // Parse argument if provided
  if (context.args && context.args[0] != '\0') {
    // Manual copy to avoid sscanf warnings and ensure bounds
    size_t len = 0;
    const char* src = context.args;
    // Skip leading spaces
    while (*src == ' ') src++;
    
    // Copy until space or end, max 15 chars
    while (*src != '\0' && *src != ' ' && len < 15) {
      modeArg[len++] = *src++;
    }
    modeArg[len] = '\0';
  }
  
  if (modeArg[0] == '\0' || strcasecmp_P(modeArg, PSTR("show")) == 0) {
    // No argument: show current mode
    char currentMode[8];
    m_apiClient.copyUploadModeString(currentMode, sizeof(currentMode));
    bool gatewayActive = m_apiClient.isLocalGatewayActive();
    char gatewayState[4];
    strncpy_P(gatewayState, gatewayActive ? PSTR("yes") : PSTR("no"), sizeof(gatewayState) - 1);
    gatewayState[sizeof(gatewayState) - 1] = '\0';
    
    Utils::ws_printf_P(context.client, PSTR("Upload Mode: %s\n"), currentMode);
    Utils::ws_printf_P(context.client, PSTR("Gateway Active: %s\n"), gatewayState);
    return;
  }

  if (strcasecmp_P(modeArg, PSTR("auto")) == 0) {
    m_apiClient.setUploadMode(UploadMode::AUTO);
    Utils::ws_printf_P(context.client, PSTR("Mode set to AUTO (automatic fallback)\n"));
  } else if (strcasecmp_P(modeArg, PSTR("cloud")) == 0) {
    m_apiClient.setUploadMode(UploadMode::CLOUD);
    Utils::ws_printf_P(context.client, PSTR("Mode set to CLOUD (forced)\n"));
  } else if (strcasecmp_P(modeArg, PSTR("edge")) == 0) {
    m_apiClient.setUploadMode(UploadMode::EDGE);
    Utils::ws_printf_P(context.client, PSTR("Mode set to EDGE (forced gateway)\n"));
  } else {
    Utils::ws_printf_P(context.client, PSTR("Invalid mode. Use: show, auto, cloud, or edge\n"));
  }
}
