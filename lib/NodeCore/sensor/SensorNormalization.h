#ifndef SENSOR_NORMALIZATION_H
#define SENSOR_NORMALIZATION_H

#include <math.h>
#include <stdint.h>

#include "sensor/SensorData.h"

namespace SensorNormalization {

struct EffectiveReading {
  float rawValue = 0.0f;
  float effectiveValue = 0.0f;
  bool isValid = false;
};

struct EffectiveSensorSnapshot {
  EffectiveReading temperature;
  EffectiveReading humidity;
  EffectiveReading light;
};

inline int32_t roundToNearestInt(float value) {
  return (value >= 0.0f) ? static_cast<int32_t>(value + 0.5f)
                         : static_cast<int32_t>(value - 0.5f);
}

inline bool normalizeTemperature(float& value) {
  if (!isfinite(value))
    return false;
  if (value < -40.0f)
    value = -40.0f;
  if (value > 100.0f)
    value = 100.0f;
  return true;
}

inline bool normalizeHumidity(float& value) {
  if (!isfinite(value) || value < 0.0f)
    return false;
  if (value > 100.0f)
    value = 100.0f;
  return true;
}

inline bool normalizeLight(float& value) {
  if (!isfinite(value) || value < 0.0f)
    return false;
  if (value > 65535.0f)
    value = 65535.0f;
  return true;
}

inline int32_t clampTemperatureTenths(int32_t value) {
  if (value < -400)
    return -400;
  if (value > 1000)
    return 1000;
  return value;
}

inline int32_t clampHumidityTenths(int32_t value) {
  if (value < 0)
    return 0;
  if (value > 1000)
    return 1000;
  return value;
}

inline uint32_t clampLightUInt(uint32_t value) {
  return value > 65535U ? 65535U : value;
}

inline EffectiveReading makeEffectiveTemperatureReading(const SensorReading& reading, float offset) {
  EffectiveReading result;
  if (!reading.isValid)
    return result;

  float raw = reading.value;
  if (!normalizeTemperature(raw))
    return result;

  float effective = raw + offset;
  if (!normalizeTemperature(effective))
    return result;

  result.rawValue = raw;
  result.effectiveValue = effective;
  result.isValid = true;
  return result;
}

inline EffectiveReading makeEffectiveHumidityReading(const SensorReading& reading, float offset) {
  EffectiveReading result;
  if (!reading.isValid)
    return result;

  float raw = reading.value;
  if (!normalizeHumidity(raw))
    return result;

  float effective = raw + offset;
  if (!normalizeHumidity(effective))
    return result;

  result.rawValue = raw;
  result.effectiveValue = effective;
  result.isValid = true;
  return result;
}

inline EffectiveReading makeEffectiveLightReading(const SensorReading& reading, float factor) {
  EffectiveReading result;
  if (!reading.isValid)
    return result;

  float raw = reading.value;
  if (!normalizeLight(raw))
    return result;

  float effective = raw * factor;
  if (!normalizeLight(effective))
    return result;

  result.rawValue = raw;
  result.effectiveValue = effective;
  result.isValid = true;
  return result;
}

inline EffectiveSensorSnapshot makeEffectiveSensorSnapshot(const SensorReading& temperatureReading,
                                                           const SensorReading& humidityReading,
                                                           const SensorReading& lightReading,
                                                           float temperatureOffset,
                                                           float humidityOffset,
                                                           float lightFactor) {
  EffectiveSensorSnapshot snapshot;
  snapshot.temperature = makeEffectiveTemperatureReading(temperatureReading, temperatureOffset);
  snapshot.humidity = makeEffectiveHumidityReading(humidityReading, humidityOffset);
  snapshot.light = makeEffectiveLightReading(lightReading, lightFactor);
  return snapshot;
}

inline bool applyTemperatureCalibration(const SensorReading& reading, float offset, int32_t& outTenths) {
  const EffectiveReading effective = makeEffectiveTemperatureReading(reading, offset);
  if (!effective.isValid)
    return false;
  outTenths = clampTemperatureTenths(roundToNearestInt(effective.effectiveValue * 10.0f));
  return true;
}

inline bool applyHumidityCalibration(const SensorReading& reading, float offset, int32_t& outTenths) {
  const EffectiveReading effective = makeEffectiveHumidityReading(reading, offset);
  if (!effective.isValid)
    return false;
  outTenths = clampHumidityTenths(roundToNearestInt(effective.effectiveValue * 10.0f));
  return true;
}

inline bool applyLightCalibration(const SensorReading& reading, float factor, uint16_t& outValue) {
  const EffectiveReading effective = makeEffectiveLightReading(reading, factor);
  if (!effective.isValid)
    return false;
  outValue = static_cast<uint16_t>(clampLightUInt(static_cast<uint32_t>(effective.effectiveValue)));
  return true;
}

}  // namespace SensorNormalization

#endif  // SENSOR_NORMALIZATION_H
