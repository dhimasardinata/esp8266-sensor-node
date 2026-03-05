#include "SetCalibrationCommand.h"

#include "ConfigManager.h"
#include "utils.h"

SetCalibrationCommand::SetCalibrationCommand(ConfigManager& configManager) : m_configManager(configManager) {}

namespace {
  bool validateCalValues(float temp, float hum, float lux, AsyncWebSocketClient* client) {
    if (lux <= 0.0f) { Utils::ws_printf(client, "[ERROR] Lux factor must be > 0."); return false; }
    if (abs(temp) > 50.0f || abs(hum) > 50.0f) { Utils::ws_printf(client, "[ERROR] Offsets too large (Max ±50)."); return false; }
    if (lux > 10.0f) { Utils::ws_printf(client, "[ERROR] Lux factor too high (Max 10)."); return false; }
    return true;
  }
}

void SetCalibrationCommand::execute(const CommandContext& context) {
  if (strnlen(context.args, 64) >= 64) {
    Utils::ws_printf(context.client, "[ERROR] Arguments too long.");
    return;
  }

  float temp, hum, lux;
  if (sscanf(context.args, "%f %f %f", &temp, &hum, &lux) != 3) {
    Utils::ws_printf(context.client, "[ERROR] Usage: setcal <temp_offset> <hum_offset> <lux_factor>");
    return;
  }

  if (!validateCalValues(temp, hum, lux, context.client)) return;

  m_configManager.setCalibration(temp, hum, lux);
  Utils::ws_printf(context.client, m_configManager.save() == ConfigStatus::OK 
    ? "[SUCCESS] Calibration saved & applied." : "[ERROR] Failed to save.");
}