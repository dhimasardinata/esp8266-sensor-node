#include "ReadSensorsCommand.h"

#include "system/ConfigManager.h"
#include "sensor/SensorManager.h"  // Concrete type for CRTP
#include "sensor/SensorNormalization.h"
#include "support/Utils.h"

ReadSensorsCommand::ReadSensorsCommand(SensorManager& sensorManager, ConfigManager& configManager)
    : m_sensorManager(sensorManager), m_configManager(configManager) {}

void ReadSensorsCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  const auto& cfg = m_configManager.getConfig();
  m_sensorManager.handle();

  const auto effective = SensorNormalization::makeEffectiveSensorSnapshot(m_sensorManager.getTemp(),
                                                                          m_sensorManager.getHumidity(),
                                                                          m_sensorManager.getLight(),
                                                                          cfg.TEMP_OFFSET,
                                                                          cfg.HUMIDITY_OFFSET,
                                                                          cfg.LUX_SCALING_FACTOR);
  char tempStatus[8];
  char humStatus[8];
  char lightStatus[8];
  strncpy_P(tempStatus, effective.temperature.isValid ? PSTR("OK") : PSTR("FAIL"), sizeof(tempStatus) - 1);
  strncpy_P(humStatus, effective.humidity.isValid ? PSTR("OK") : PSTR("FAIL"), sizeof(humStatus) - 1);
  strncpy_P(lightStatus, effective.light.isValid ? PSTR("OK") : PSTR("FAIL"), sizeof(lightStatus) - 1);
  tempStatus[sizeof(tempStatus) - 1] = '\0';
  humStatus[sizeof(humStatus) - 1] = '\0';
  lightStatus[sizeof(lightStatus) - 1] = '\0';

  Utils::ws_printf_P(context.client, PSTR("Sensor Readings:\n"));
  Utils::ws_printf_P(context.client,
                     PSTR("  Temp: %s (%.1fC -> %.1fC)\n"),
                     tempStatus,
                     effective.temperature.rawValue,
                     effective.temperature.effectiveValue);
  Utils::ws_printf_P(context.client,
                     PSTR("  Hum: %s (%.1f%% -> %.1f%%)\n"),
                     humStatus,
                     effective.humidity.rawValue,
                     effective.humidity.effectiveValue);
  Utils::ws_printf_P(context.client,
                     PSTR("  Light: %s (%.0f -> %.0f lux)\n"),
                     lightStatus,
                     effective.light.rawValue,
                     effective.light.effectiveValue);
}
