/**
 * @file calibration.h
 * @brief Sensor calibration defaults (per-node)
 */
#ifndef COMPILED_CALIBRATION_DEFAULTS_H
#define COMPILED_CALIBRATION_DEFAULTS_H

#include "node_config.h"
#ifndef NODE_ID
#error "NODE_ID is not defined! Please define it in platformio.ini"
#endif

namespace CompiledDefaults {
  // Default calibration values - customize per node
  constexpr float TEMP_OFFSET = 0.0f;
  constexpr float HUMIDITY_OFFSET = 0.0f;
  constexpr float LUX_SCALING_FACTOR = 1.0f;
}  // namespace CompiledDefaults
#endif  // COMPILED_CALIBRATION_DEFAULTS_H
