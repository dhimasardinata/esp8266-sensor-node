#include "GetConfigCommand.h"

#include "ConfigManager.h"
#include "utils.h"

GetConfigCommand::GetConfigCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void GetConfigCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  const AppConfig& cfg = m_configManager.getConfig();
  const char* dataUrl = m_configManager.getDataUploadUrl();
  const char* otaUrl = REDACTED
  Utils::ws_printf(context.client,
    "Current Configuration:\n"
    "  Auth Token         : REDACTED
    "  Portal Password    : REDACTED
    "  Upload Interval    : %lu ms\n  Sample Interval    : %lu ms\n"
    "  Cache Send Interval: %lu ms\n  SW WDT Timeout     : %lu ms\n",
    dataUrl, otaUrl,
    cfg.IS_PROVISIONED() ? "Yes" : "No",
    (unsigned long)cfg.DATA_UPLOAD_INTERVAL_MS, (unsigned long)cfg.SENSOR_SAMPLE_INTERVAL_MS,
    (unsigned long)cfg.CACHE_SEND_INTERVAL_MS, (unsigned long)cfg.SOFTWARE_WDT_TIMEOUT_MS);

  m_configManager.releaseStrings();
}
