#include "SensorManager.h"

#include <Wire.h>

#include "hardware_pins.h"
#include "Logger.h"

SensorManager::SensorManager()
    : m_lightMeter(AppConstants::BH1750_I2C_ADDR),
      m_currentState(State::INITIALIZING),
      m_shtReadTimer(AppConstants::SHT_READ_INTERVAL_MS),
      m_actionTimer(AppConstants::SENSOR_INIT_RETRY_INTERVAL_MS),
      m_shtFailureNotified(false),
      m_bh1750FailureNotified(false) {}

void SensorManager::init() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  delay(AppConstants::I2C_SETTLE_DELAY_MS);

  m_currentState = State::INITIALIZING;
  m_actionTimer.reset();
  LOG_INFO("SENSOR", F("Initialization scheduled (non-blocking)."));
}

void SensorManager::handle() {
  switch (m_currentState) {
    case State::INITIALIZING: handleInitializing(); break;
    case State::RUNNING:      handleRunning(); break;
    case State::RECOVERY:     handleRecovery(); break;
  }
}

void SensorManager::handleInitializing() {
  if (!m_actionTimer.hasElapsed()) return;
  attemptSensorInitOrRecovery();
  
  if (m_shtState.isOk && m_bh1750State.isOk) {
    LOG_INFO("SENSOR", F("All sensors initialized."));
    m_currentState = State::RUNNING;
    m_initFailureCount = 0;
    return;
  }
  
  if (++m_initFailureCount >= 5) {
    LOG_WARN("SENSOR", F("Init stuck. Triggering I2C Bus Recovery..."));
    recoverI2CBus();
    m_initFailureCount = 0;
    delay(AppConstants::I2C_SETTLE_DELAY_MS);
  }
  if (m_actionTimer.getInterval() != AppConstants::SENSOR_SLOW_RETRY_INTERVAL_MS)
    m_actionTimer.setInterval(AppConstants::SENSOR_SLOW_RETRY_INTERVAL_MS);
}

void SensorManager::handleRunning() {
  updateShtData();
  updateBh1750Data();
  if (!m_shtState.isOk || !m_bh1750State.isOk) {
    LOG_WARN("SENSOR", F("Failure detected. Entering RECOVERY."));
    m_currentState = State::RECOVERY;
    m_actionTimer.setInterval(AppConstants::SENSOR_RECOVERY_INTERVAL_MS);
    m_actionTimer.reset();
  }
}

void SensorManager::handleRecovery() {
  if (!m_actionTimer.hasElapsed()) return;
  recoverI2CBus();
  attemptSensorInitOrRecovery();
  if (m_shtState.isOk && m_bh1750State.isOk) {
    LOG_INFO("SENSOR-REC", F("All sensors recovered."));
    m_currentState = State::RUNNING;
    m_actionTimer.setInterval(AppConstants::SENSOR_INIT_RETRY_INTERVAL_MS);
  } else {
    m_actionTimer.reset();
  }
}

void SensorManager::updateShtData() {
  if (!m_shtState.isOk || !m_shtReadTimer.hasElapsed()) {
    return;
  }
  if (m_sht.readSample()) {
    m_shtState.failureCount = 0;
    m_temperature = m_sht.getTemperature();
    m_humidity = m_sht.getHumidity();
    m_shtFailureNotified = false;
  } else {
    m_shtState.failureCount++;
    if (!m_shtFailureNotified) {
      LOG_ERROR("SENSOR", F("Failed to read from SHT sensor. Will retry silently."));
      m_shtFailureNotified = true;
    }
    // Invalidate immediately on failure (Raw Mode)
    m_temperature = INVALID_TEMP;
    m_humidity = INVALID_HUMIDITY;
    
    if (m_shtState.failureCount >= AppConstants::SENSOR_MAX_FAILURES) {
      m_shtState.isOk = false;
      LOG_ERROR("SENSOR", F("SHT sensor marked as offline. Will attempt recovery."));
    }
  }
}

void SensorManager::updateBh1750Data() {
  if (!m_bh1750State.isOk) {
    return;
  }
  float lux = m_lightMeter.readLightLevel();
  if (lux >= 0) {
    m_bh1750State.failureCount = 0;
    m_lightLevel = lux;
    m_bh1750FailureNotified = false;
  } else {
    m_bh1750State.failureCount++;
    if (!m_bh1750FailureNotified) {
      LOG_ERROR("SENSOR", F("Failed to read from BH1750. Code: %.0f. Will retry silently."), lux);
      m_bh1750FailureNotified = true;
    }
    // Invalidate immediately on failure
    m_lightLevel = INVALID_LUX;
    
    if (m_bh1750State.failureCount >= AppConstants::SENSOR_MAX_FAILURES) {
      m_bh1750State.isOk = false;
      LOG_ERROR("SENSOR", F("BH1750 sensor marked as offline. Will attempt recovery."));
    }
  }
}

bool SensorManager::tryInitSht() {
  if (!m_sht.init()) {
    if (!m_shtFailureNotified) {
      LOG_ERROR("SENSOR", F("SHT Sensor: Init failed."));
      m_shtFailureNotified = true;
    }
    return false;
  }
  m_sht.setAccuracy(SHTSensor::SHT_ACCURACY_MEDIUM);
  if (m_shtFailureNotified) LOG_INFO("RECOVERY", F("SHT: RECOVERED"));
  m_shtState = {true, 0};
  m_shtFailureNotified = false;
  return true;
}

bool SensorManager::tryInitBh1750() {
  if (!m_lightMeter.begin()) {
    if (!m_bh1750FailureNotified) {
      LOG_ERROR("SENSOR", F("BH1750: Init failed."));
      m_bh1750FailureNotified = true;
    }
    return false;
  }
  if (m_bh1750FailureNotified) LOG_INFO("RECOVERY", F("BH1750: RECOVERED"));
  m_bh1750State = {true, 0};
  m_bh1750FailureNotified = false;
  delay(AppConstants::BH1750_INIT_DELAY_MS);
  return true;
}

void SensorManager::attemptSensorInitOrRecovery() {
  if (!m_shtState.isOk) tryInitSht();
  if (!m_bh1750State.isOk) tryInitBh1750();
}

void SensorManager::recoverI2CBus() {
  LOG_WARN("I2C-REC", F("Attempting to recover I2C bus..."));
  pinMode(PIN_I2C_SDA, INPUT_PULLUP);
  if (digitalRead(PIN_I2C_SDA) == LOW) {
    LOG_WARN("I2C-REC", F("SDA line is stuck low. Generating clock pulses on SCL..."));
    pinMode(PIN_I2C_SCL, OUTPUT);
    for (int i = 0; i < 9; i++) {
      digitalWrite(PIN_I2C_SCL, LOW);
      delayMicroseconds(5);
      digitalWrite(PIN_I2C_SCL, HIGH);
      delayMicroseconds(5);
    }
  }
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  static unsigned long lastInitLog = 0;
  if (millis() - lastInitLog > 60000) {
    LOG_INFO("I2C-REC", F("I2C bus re-initialized."));
    lastInitLog = millis();
  }
}

SensorReading SensorManager::getTemp() const {
  bool isValid = m_shtState.isOk && (m_temperature != INVALID_TEMP);
  return {m_temperature, isValid};
}

SensorReading SensorManager::getHumidity() const {
  bool isValid = m_shtState.isOk && (m_humidity != INVALID_HUMIDITY);
  return {m_humidity, isValid};
}

SensorReading SensorManager::getLight() const {
  bool isValid = m_bh1750State.isOk && (m_lightLevel != INVALID_LUX);
  return {m_lightLevel, isValid};
}

bool SensorManager::getShtStatus() const {
  return m_shtState.isOk;
}

bool SensorManager::getBh1750Status() const {
  return m_bh1750State.isOk;
}