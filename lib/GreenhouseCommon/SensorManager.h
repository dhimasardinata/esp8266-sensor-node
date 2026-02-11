#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <BH1750.h>
#include <IntervalTimer.h>
#include <SHTSensor.h>
#include <sensor_data.h>

#include "ISensorManager.h"
#include "constants.h"

// --- Sensor Constants ---
constexpr float INVALID_TEMP = -999.0f;
constexpr float INVALID_HUMIDITY = -999.0f;
constexpr float INVALID_LUX = -1.0f;

struct SensorState {
  bool isOk = false;
  uint8_t failureCount = 0;
};

// ============================================================================
// SensorManager with CRTP (Zero Virtual Overhead)
// ============================================================================
// Uses CRTP for compile-time polymorphism:
// - No vtable pointer (saves 4 bytes)
// - All method calls are inlined by the compiler
// - No indirect jumps

class SensorManager : public ISensorManager<SensorManager> {
  // Allow CRTP base to call our Impl methods
  friend class ISensorManager<SensorManager>;
  
public:
  SensorManager();

  void init();

  // Public interface (can be called directly or via CRTP base)
  void handleImpl();
  void pause();
  void resume();
  SensorReading getTempImpl() const;
  SensorReading getHumidityImpl() const;
  SensorReading getLightImpl() const;
  bool getShtStatusImpl() const;
  bool getBh1750StatusImpl() const;

private:
  enum class State { INITIALIZING, RUNNING, RECOVERY, PAUSED };

  void attemptSensorInitOrRecovery();
  void IRAM_ATTR recoverI2CBus();  // In IRAM for timing precision
  void updateShtData();
  void updateBh1750Data();
  void handleInitializing();
  void handleRunning();
  void handleRecovery();
  bool tryInitSht();
  bool tryInitBh1750();

  BH1750 m_lightMeter;
  State m_currentState;
  SHTSensor m_sht;

  IntervalTimer m_shtReadTimer;
  IntervalTimer m_actionTimer;
  uint8_t m_initFailureCount = 0;

  SensorState m_shtState;
  SensorState m_bh1750State;

  float m_temperature = INVALID_TEMP;
  float m_humidity = INVALID_HUMIDITY;
  float m_lightLevel = INVALID_LUX;

  bool m_shtFailureNotified = false;
  bool m_bh1750FailureNotified = false;
  
  unsigned long m_lastI2CLogTime = 0;
};

#endif  // SENSOR_MANAGER_H
