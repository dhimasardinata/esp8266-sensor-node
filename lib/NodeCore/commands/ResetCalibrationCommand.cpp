#include "ResetCalibrationCommand.h"

#include "system/ConfigManager.h"
#include "config/calibration.h"
#include "support/Utils.h"

ResetCalibrationCommand::ResetCalibrationCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void ResetCalibrationCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) {
    return;
  }

  m_configManager.setCalibration(
      CompiledDefaults::TEMP_OFFSET, CompiledDefaults::HUMIDITY_OFFSET, CompiledDefaults::LUX_SCALING_FACTOR);
  const ConfigStatus status = m_configManager.save();
  m_configManager.releaseStrings();
  if (status == ConfigStatus::OK) {
    Utils::ws_printf_P(context.client,
                       PSTR("Calibration values have been reset to firmware defaults and saved. Settings are being applied live."));
  } else {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Failed to save reset calibration values."));
  }
}
