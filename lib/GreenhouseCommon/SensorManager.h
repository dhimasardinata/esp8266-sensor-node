#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <BH1750.h>
#include <IntervalTimer.h>
#include <SHTSensor.h>
#include <sensor_data.h>

#include "ISensorManager.h"
#include "constants.h"

// --- Konstanta Sensor ---
constexpr float INVALID_TEMP = -999.0f;
constexpr float INVALID_HUMIDITY = -999.0f;
constexpr float INVALID_LUX = -1.0f;

struct SensorState {
  bool isOk = false;
  int failureCount = 0;
};

class SensorManager : public ISensorManager {
public:
  SensorManager();

  // --- MODIFICATION: Add 'override' keyword for clarity and safety ---
  void handle() override;
  SensorReading getTemp() const override;
  SensorReading getHumidity() const override;
  SensorReading getLight() const override;
  bool getShtStatus() const override;
  bool getBh1750Status() const override;

  void init();  // Init is not part of the interface, it's an implementation detail

private:
  enum class State { INITIALIZING, RUNNING, RECOVERY };

  void attemptSensorInitOrRecovery();
  void recoverI2CBus();
  void updateShtData();
  void updateBh1750Data();

  BH1750 m_lightMeter;
  State m_currentState;  // After m_lightMeter to match constructor order
  SHTSensor m_sht;

  IntervalTimer m_shtReadTimer;
  IntervalTimer m_actionTimer;
  int m_initFailureCount = 0;

  SensorState m_shtState;
  SensorState m_bh1750State;

  float m_temperature = INVALID_TEMP;
  float m_humidity = INVALID_HUMIDITY;
  float m_lightLevel = INVALID_LUX;

  bool m_shtFailureNotified = false;
  bool m_bh1750FailureNotified = false;
};

#endif  // SENSOR_MANAGER_H