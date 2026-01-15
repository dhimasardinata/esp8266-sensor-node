#ifndef I_SENSOR_MANAGER_H
#define I_SENSOR_MANAGER_H

#include "sensor_data.h"

/**
 * @brief Interface for any class that provides sensor readings.
 * @details This allows for dependency injection, enabling the use of mock sensors for testing.
 */
class ISensorManager {
public:
  virtual ~ISensorManager() = default;

  // Pure virtual functions define the interface
  virtual void handle() = 0;
  virtual SensorReading getTemp() const = 0;
  virtual SensorReading getHumidity() const = 0;
  virtual SensorReading getLight() const = 0;
  virtual bool getShtStatus() const = 0;
  virtual bool getBh1750Status() const = 0;
};

#endif  // I_SENSOR_MANAGER_H