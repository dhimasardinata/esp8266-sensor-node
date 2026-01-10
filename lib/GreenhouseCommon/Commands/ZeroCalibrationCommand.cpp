#include "ZeroCalibrationCommand.h"

#include "ConfigManager.h"
#include "calibration.h"
#include "utils.h"

ZeroCalibrationCommand::ZeroCalibrationCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void ZeroCalibrationCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) {
    return;
  }

  m_configManager.setCalibration(0.0f, 0.0f, 1.0f);

  if (m_configManager.save() == ConfigStatus::OK) {
    const auto& cfg = m_configManager.getConfig();
    Utils::ws_printf(context.client,
                     "\n[SUCCESS] Calibration has been reset to zero, saved and is being applied live.\n"
                     "\n--- Sensor Calibration Settings ---\n"
                     "Setting            | Current (Runtime) | Compiled Default\n"
                     "-------------------|-------------------|-----------------\n"
                     "Temp Offset (C)    | %-17.2f | %.2f\n"
                     "Humidity Offset (%%) | %-17.2f | %.2f\n"
                     "Lux Scaling Factor | %-17.2f | %.2f\n"
                     "---------------------------------------------------------\n",
                     cfg.TEMP_OFFSET,
                     CompiledDefaults::TEMP_OFFSET,
                     cfg.HUMIDITY_OFFSET,
                     CompiledDefaults::HUMIDITY_OFFSET,
                     cfg.LUX_SCALING_FACTOR,
                     CompiledDefaults::LUX_SCALING_FACTOR);
  } else {
    Utils::ws_printf(context.client, "\n[ERROR] Failed to save zeroed calibration settings.\n");
  }
}