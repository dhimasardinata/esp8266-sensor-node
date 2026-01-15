#include "ResetCalibrationCommand.h"

#include "ConfigManager.h"
#include "calibration.h"
#include "utils.h"

ResetCalibrationCommand::ResetCalibrationCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void ResetCalibrationCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) {
    return;
  }

  m_configManager.setCalibration(
      CompiledDefaults::TEMP_OFFSET, CompiledDefaults::HUMIDITY_OFFSET, CompiledDefaults::LUX_SCALING_FACTOR);
  if (m_configManager.save() == ConfigStatus::OK) {
    Utils::ws_printf(context.client,
                     "Calibration values have been reset to firmware defaults and saved. Settings are being "
                     "applied live.");
  } else {
    Utils::ws_printf(context.client, "[ERROR] Failed to save reset calibration values.");
  }
}