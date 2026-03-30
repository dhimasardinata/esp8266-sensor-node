#include "ZeroCalibrationCommand.h"

#include "system/ConfigManager.h"
#include "config/calibration.h"
#include "support/Utils.h"

ZeroCalibrationCommand::ZeroCalibrationCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void ZeroCalibrationCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  m_configManager.setCalibration(0.0f, 0.0f, 1.0f);

  const ConfigStatus status = m_configManager.save();
  m_configManager.releaseStrings();
  if (status != ConfigStatus::OK) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Failed to save.\n"));
    return;
  }

  const auto& c = m_configManager.getConfig();
  Utils::ws_printf_P(context.client,
                     PSTR("[OK] Calibration reset.\n"
                          "Temp: %.2f (def %.2f) | Hum: %.2f (def %.2f) | Lux: %.2f (def %.2f)\n"),
                     c.TEMP_OFFSET, CompiledDefaults::TEMP_OFFSET,
                     c.HUMIDITY_OFFSET, CompiledDefaults::HUMIDITY_OFFSET,
                     c.LUX_SCALING_FACTOR, CompiledDefaults::LUX_SCALING_FACTOR);
}
