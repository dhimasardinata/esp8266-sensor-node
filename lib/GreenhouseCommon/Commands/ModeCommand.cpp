#include "ModeCommand.h"

#include "ApiClient.h"
#include "CommandContext.h"
#include "utils.h"

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
  
  if (modeArg[0] == '\0') {
    // No argument: show current mode
    const char* currentMode = m_apiClient.getUploadModeString();
    bool gatewayActive = m_apiClient.isLocalGatewayActive();
    
    Utils::ws_printf(context.client, "Upload Mode: %s", currentMode);
    Utils::ws_printf(context.client, "Gateway Active: %s", gatewayActive ? "yes" : "no");
    return;
  }

  if (strcasecmp(modeArg, "auto") == 0) {
    m_apiClient.setUploadMode(UploadMode::AUTO);
    Utils::ws_printf(context.client, "Mode set to AUTO (automatic fallback)");
  } else if (strcasecmp(modeArg, "cloud") == 0) {
    m_apiClient.setUploadMode(UploadMode::CLOUD);
    Utils::ws_printf(context.client, "Mode set to CLOUD (forced)");
  } else if (strcasecmp(modeArg, "edge") == 0) {
    m_apiClient.setUploadMode(UploadMode::EDGE);
    Utils::ws_printf(context.client, "Mode set to EDGE (forced gateway)");
  } else {
    Utils::ws_printf(context.client, "Invalid mode. Use: auto, cloud, or edge");
  }
}
