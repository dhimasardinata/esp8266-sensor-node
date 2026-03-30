#include "SetCalibrationCommand.h"

#include <math.h>

#include "config/constants.h"
#include "system/ConfigManager.h"
#include "support/Utils.h"

SetCalibrationCommand::SetCalibrationCommand(ConfigManager& configManager) : m_configManager(configManager) {}

namespace {
  bool validateCalValues(float temp, float hum, float lux, AsyncWebSocketClient* client) {
    if (lux <= 0.0f) { Utils::ws_printf_P(client, PSTR("[ERROR] Lux factor must be > 0.")); return false; }
    if (fabsf(temp) > AppConstants::CALIBRATION_OFFSET_MAX || fabsf(hum) > AppConstants::CALIBRATION_OFFSET_MAX) {
      Utils::ws_printf_P(client, PSTR("[ERROR] Offsets too large (exceeds firmware limit)."));
      return false;
    }
    if (lux > AppConstants::LUX_FACTOR_MAX) {
      Utils::ws_printf_P(client, PSTR("[ERROR] Lux factor too high (exceeds firmware limit)."));
      return false;
    }
    return true;
  }
}

void SetCalibrationCommand::execute(const CommandContext& context) {
  if (strnlen(context.args, 64) >= 64) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Arguments too long."));
    return;
  }

  float temp, hum, lux;
  if (sscanf(context.args, "%f %f %f", &temp, &hum, &lux) != 3) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Usage: setcal <temp_offset> <hum_offset> <lux_factor>"));
    return;
  }

  if (!validateCalValues(temp, hum, lux, context.client)) return;

  m_configManager.setCalibration(temp, hum, lux);
  const ConfigStatus status = m_configManager.save();
  m_configManager.releaseStrings();
  Utils::ws_printf_P(context.client,
                     status == ConfigStatus::OK ? PSTR("[SUCCESS] Calibration saved & applied.")
                                                : PSTR("[ERROR] Failed to save."));
}
