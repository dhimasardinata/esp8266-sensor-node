#include "GetCalibrationCommand.h"

#include "system/ConfigManager.h"
#include "config/calibration.h"
#include "support/Utils.h"

GetCalibrationCommand::GetCalibrationCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void GetCalibrationCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  const auto& cfg = m_configManager.getConfig();
  Utils::ws_printf_P(context.client, PSTR("\n--- Sensor Calibration Settings ---\n"));
  Utils::ws_printf_P(context.client, PSTR("Setting            | Current (Runtime) | Compiled Default\n"));
  Utils::ws_printf_P(context.client, PSTR("-------------------|-------------------|-----------------\n"));
  Utils::ws_printf_P(context.client,
                     PSTR("Temp Offset (C)    | %-17.2f | %.2f\n"),
                     cfg.TEMP_OFFSET,
                     CompiledDefaults::TEMP_OFFSET);
  Utils::ws_printf_P(context.client,
                     PSTR("Humidity Offset (%%) | %-17.2f | %.2f\n"),
                     cfg.HUMIDITY_OFFSET,
                     CompiledDefaults::HUMIDITY_OFFSET);
  Utils::ws_printf_P(context.client,
                     PSTR("Lux Scaling Factor | %-17.2f | %.2f\n"),
                     cfg.LUX_SCALING_FACTOR,
                     CompiledDefaults::LUX_SCALING_FACTOR);
  Utils::ws_printf_P(context.client, PSTR("---------------------------------------------------------\n"));
}
