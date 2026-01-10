#include "ReadSensorsCommand.h"

#include "ConfigManager.h"
#include "ISensorManager.h"
#include "utils.h"

ReadSensorsCommand::ReadSensorsCommand(ISensorManager& sensorManager, ConfigManager& configManager)
    : m_sensorManager(sensorManager), m_configManager(configManager) {}

void ReadSensorsCommand::execute(const CommandContext& context) {
  const auto& cfg = m_configManager.getConfig();
  m_sensorManager.handle();

  SensorReading temp = m_sensorManager.getTemp();
  SensorReading hum = m_sensorManager.getHumidity();
  SensorReading light = m_sensorManager.getLight();

  if (!context.client || !context.client->canSend()) {
    return;
  }

  Utils::ws_printf(context.client,
                   "Sensor Readings:\n"
                   "  Temperature: %s (Raw: %.1f C, Calibrated: %.1f C)\n"
                   "  Humidity   : %s (Raw: %.1f %%, Calibrated: %.1f %%)\n"
                   "  Light      : %s (Raw: %.0f lux, Calibrated: %.0f lux)",
                   temp.isValid ? "OK" : "FAIL",
                   temp.value,
                   temp.value + cfg.TEMP_OFFSET,
                   hum.isValid ? "OK" : "FAIL",
                   hum.value,
                   hum.value + cfg.HUMIDITY_OFFSET,
                   light.isValid ? "OK" : "FAIL",
                   light.value,
                   light.value * cfg.LUX_SCALING_FACTOR);
}