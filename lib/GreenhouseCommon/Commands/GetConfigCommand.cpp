#include "GetConfigCommand.h"

#include "ConfigManager.h"
#include "utils.h"

GetConfigCommand::GetConfigCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void GetConfigCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  const AppConfig& cfg = m_configManager.getConfig();
  Utils::ws_printf(context.client,
    "Current Configuration:\n"
    "  Auth Token         : [HIDDEN]\n  Data URL           : %s\n  OTA URL Base       : %s\n"
    "  Portal Password    : [HIDDEN]\n  Provisioned        : %s\n"
    "  Upload Interval    : %lu ms\n  Sample Interval    : %lu ms\n"
    "  Cache Send Interval: %lu ms\n  SW WDT Timeout     : %lu ms\n",
    cfg.DATA_UPLOAD_URL.data(), cfg.FW_VERSION_CHECK_URL_BASE.data(),
    cfg.IS_PROVISIONED ? "Yes" : "No",
    cfg.DATA_UPLOAD_INTERVAL_MS, cfg.SENSOR_SAMPLE_INTERVAL_MS,
    cfg.CACHE_SEND_INTERVAL_MS, cfg.SOFTWARE_WDT_TIMEOUT_MS);
}