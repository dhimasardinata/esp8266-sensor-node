#include "SetCalibrationCommand.h"

#include "ConfigManager.h"
#include "utils.h"

SetCalibrationCommand::SetCalibrationCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void SetCalibrationCommand::execute(const CommandContext& context) {
  float temp, hum, lux;

  // Use strlen instead of .length()
  if (strlen(context.args) >= 64) {
    Utils::ws_printf(context.client, "[ERROR] Arguments too long.");
    return;
  }

  // Use sscanf directly on the const char*
  int parsed = sscanf(context.args, "%f %f %f", &temp, &hum, &lux);

  if (parsed != 3) {
    Utils::ws_printf(context.client,
                     "[ERROR] Invalid format. Usage: setcal <temp_offset> <humidity_offset> <lux_factor>\n"
                     "Example: setcal -0.5 2.1 1.15");
    return;
  }

  // Safety Checks
  if (lux <= 0.0f) {
    Utils::ws_printf(context.client, "[ERROR] Lux factor must be positive (> 0.0).");
    return;
  }
  if (abs(temp) > 50.0f || abs(hum) > 50.0f) {
    Utils::ws_printf(context.client,
                     "[ERROR] Calibration offsets are too large (Max +/- 50.0). Check your values.");
    return;
  }
  if (lux > 10.0f) {
    Utils::ws_printf(context.client, "[ERROR] Lux factor too high (Max 10.0).");
    return;
  }

  m_configManager.setCalibration(temp, hum, lux);
  if (m_configManager.save() == ConfigStatus::OK) {
    Utils::ws_printf(context.client,
                     "[SUCCESS] Calibration settings updated and saved. Settings are being applied live.");
  } else {
    Utils::ws_printf(context.client, "[ERROR] Failed to save calibration settings.");
  }
}