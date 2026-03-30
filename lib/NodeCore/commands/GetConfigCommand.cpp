#include "GetConfigCommand.h"

#include <cstring>

#include "system/ConfigManager.h"
#include "support/Utils.h"

#ifndef FACTORY_API_TOKEN
#define FACTORY_API_TOKEN "REDACTED"
#endif

#ifndef FACTORY_OTA_TOKEN
#define FACTORY_OTA_TOKEN "REDACTED"
#endif

GetConfigCommand::GetConfigCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void GetConfigCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  const AppConfig& cfg = m_configManager.getConfig();
  const char* dataUrl = m_configManager.getDataUploadUrl();
  const char* otaUrl = REDACTED
  const char* gatewayHostGh1 = m_configManager.getGatewayHost(1);
  const char* gatewayHostGh2 = m_configManager.getGatewayHost(2);
  const char* gatewayIpGh1 = m_configManager.getGatewayIp(1);
  const char* gatewayIpGh2 = m_configManager.getGatewayIp(2);
  const char* displayDataUrl = (dataUrl && dataUrl[0] != '\0') ? dataUrl : "<none>";
  const char* displayOtaUrl = REDACTED
  const char* displayGatewayHostGh1 = (gatewayHostGh1 && gatewayHostGh1[0] != '\0') ? gatewayHostGh1 : "<none>";
  const char* displayGatewayHostGh2 = (gatewayHostGh2 && gatewayHostGh2[0] != '\0') ? gatewayHostGh2 : "<none>";
  const char* displayGatewayIpGh1 = (gatewayIpGh1 && gatewayIpGh1[0] != '\0') ? gatewayIpGh1 : "<none>";
  const char* displayGatewayIpGh2 = (gatewayIpGh2 && gatewayIpGh2[0] != '\0') ? gatewayIpGh2 : "<none>";
  const char* uploadToken = REDACTED
  const bool uploadEmpty = (!uploadToken || uploadToken[0] == '\0');
  const bool uploadFactory = (!uploadEmpty && strcmp(uploadToken, FACTORY_API_TOKEN) == 0);
  const char* uploadState = uploadEmpty ? "empty" : (uploadFactory ? "factory-default" : "set");
  const char* otaState = REDACTED
  const char* effectiveUpload = nullptr;
  if (!uploadEmpty) {
    effectiveUpload = "upload-slot";
  } else if (m_configManager.hasOtaTokenOverride()) {
    effectiveUpload = "ota-override";
  } else if (FACTORY_OTA_TOKEN[0] != REDACTED
    effectiveUpload = "factory-ota";
  } else {
    effectiveUpload = "none";
  }
  const char* effectiveOta = REDACTED
  if (m_configManager.hasOtaTokenOverride()) {
    effectiveOta = REDACTED
  } else if (FACTORY_OTA_TOKEN[0] != REDACTED
    effectiveOta = REDACTED
  } else if (!uploadEmpty) {
    effectiveOta = REDACTED
  } else {
    effectiveOta = REDACTED
  }
  char provisioned[4];
  strncpy_P(provisioned, cfg.IS_PROVISIONED() ? PSTR("Yes") : PSTR("No"), sizeof(provisioned) - 1);
  provisioned[sizeof(provisioned) - 1] = '\0';
  Utils::ws_printf_P(context.client, PSTR("Current Configuration:\n"));
  Utils::ws_printf_P(context.client, PSTR("  Upload Token       : [HIDDEN] (%s)\n"), uploadState);
  Utils::ws_printf_P(context.client, PSTR("  OTA Token          : %s\n"), otaState);
  Utils::ws_printf_P(context.client, PSTR("  Upload Token Uses  : %s\n"), effectiveUpload);
  Utils::ws_printf_P(context.client, PSTR("  OTA Token Uses     : %s\n"), effectiveOta);
  Utils::ws_printf_P(context.client, PSTR("  Data URL           : %s\n"), displayDataUrl);
  Utils::ws_printf_P(context.client, PSTR("  OTA URL Base       : %s\n"), displayOtaUrl);
  Utils::ws_printf_P(context.client, PSTR("  Gateway GH1 Host   : %s\n"), displayGatewayHostGh1);
  Utils::ws_printf_P(context.client, PSTR("  Gateway GH1 IP     : %s\n"), displayGatewayIpGh1);
  Utils::ws_printf_P(context.client, PSTR("  Gateway GH2 Host   : %s\n"), displayGatewayHostGh2);
  Utils::ws_printf_P(context.client, PSTR("  Gateway GH2 IP     : %s\n"), displayGatewayIpGh2);
  Utils::ws_printf_P(context.client, PSTR("  Portal Password    : [HIDDEN]\n"));
  Utils::ws_printf_P(context.client, PSTR("  Provisioned        : %s\n"), provisioned);
  Utils::ws_printf_P(context.client, PSTR("  Upload Interval    : %lu ms\n"), (unsigned long)cfg.DATA_UPLOAD_INTERVAL_MS);
  Utils::ws_printf_P(context.client, PSTR("  Sample Interval    : %lu ms\n"), (unsigned long)cfg.SENSOR_SAMPLE_INTERVAL_MS);
  Utils::ws_printf_P(context.client, PSTR("  Cache Send Interval: %lu ms\n"), (unsigned long)cfg.CACHE_SEND_INTERVAL_MS);
  Utils::ws_printf_P(context.client, PSTR("  SW WDT Timeout     : %lu ms\n"), (unsigned long)cfg.SOFTWARE_WDT_TIMEOUT_MS);

  m_configManager.releaseStrings();
}
