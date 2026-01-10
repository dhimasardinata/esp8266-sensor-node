#include "GetCalibrationCommand.h"

#include "ConfigManager.h"
#include "calibration.h"
#include "utils.h"

GetCalibrationCommand::GetCalibrationCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void GetCalibrationCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) {
    return;
  }

  const auto& cfg = m_configManager.getConfig();

  Utils::ws_printf(context.client,
                   "\n--- Sensor Calibration Settings ---\n"
                   "Setting            | Current (Runtime) | Compiled Default\n"
                   "-------------------|-------------------|-----------------\n"
                   "Temp Offset (C)    | %-17.2f | %.2f\n"
                   "Humidity Offset (%%) | %-17.2f | %.2f\n"
                   "Lux Scaling Factor | %-17.2f | %.2f\n"
                   "---------------------------------------------------------\n"
                   "Note: 'Current' is used by the system. 'Default' is the value from compilation.\n",
                   cfg.TEMP_OFFSET,
                   CompiledDefaults::TEMP_OFFSET,
                   cfg.HUMIDITY_OFFSET,
                   CompiledDefaults::HUMIDITY_OFFSET,
                   cfg.LUX_SCALING_FACTOR,
                   CompiledDefaults::LUX_SCALING_FACTOR);
}