#include "ReadSensorsCommand.h"

#include "ConfigManager.h"
#include "SensorManager.h"  // Concrete type for CRTP
#include "utils.h"

ReadSensorsCommand::ReadSensorsCommand(SensorManager& sensorManager, ConfigManager& configManager)
    : m_sensorManager(sensorManager), m_configManager(configManager) {}

void ReadSensorsCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  const auto& cfg = m_configManager.getConfig();
  m_sensorManager.handle();

  auto t = m_sensorManager.getTemp();
  auto h = m_sensorManager.getHumidity();
  auto l = m_sensorManager.getLight();

  Utils::ws_printf(context.client,
    "Sensor Readings:\n"
    "  Temp: %s (%.1fC -> %.1fC)\n  Hum: %s (%.1f%% -> %.1f%%)\n  Light: %s (%.0f -> %.0f lux)",
    t.isValid ? "OK" : "FAIL", t.value, t.value + cfg.TEMP_OFFSET,
    h.isValid ? "OK" : "FAIL", h.value, h.value + cfg.HUMIDITY_OFFSET,
    l.isValid ? "OK" : "FAIL", l.value, l.value * cfg.LUX_SCALING_FACTOR);
}