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
    case State::INITIALIZING: {
      if (m_actionTimer.hasElapsed()) {
        attemptSensorInitOrRecovery();
        if (m_shtState.isOk && m_bh1750State.isOk) {
          LOG_INFO("SENSOR", F("All sensors initialized successfully."));
          m_currentState = State::RUNNING;
          m_initFailureCount = 0; // Reset counter on success
        } else {
          m_initFailureCount++;
          if (m_initFailureCount >= 5) {
            LOG_WARN("SENSOR", F("Init stuck. Triggering I2C Bus Recovery..."));
            recoverI2CBus();
            m_initFailureCount = 0;
            // Short delay to allow electrical settling
            delay(AppConstants::I2C_SETTLE_DELAY_MS);
          }

          if (m_actionTimer.getInterval() != AppConstants::SENSOR_SLOW_RETRY_INTERVAL_MS) {
            m_actionTimer.setInterval(AppConstants::SENSOR_SLOW_RETRY_INTERVAL_MS);
          }
        }
      }
      break;
    }

    case State::RUNNING: {
      updateShtData();
      updateBh1750Data();
      if (!m_shtState.isOk || !m_bh1750State.isOk) {
        LOG_WARN("SENSOR", F("Failure detected. Entering RECOVERY state."));
        m_currentState = State::RECOVERY;
        m_actionTimer.setInterval(AppConstants::SENSOR_RECOVERY_INTERVAL_MS);
        m_actionTimer.reset();
      }
      break;
    }

    case State::RECOVERY: {
      if (m_actionTimer.hasElapsed()) {
        recoverI2CBus();
        attemptSensorInitOrRecovery();
        if (m_shtState.isOk && m_bh1750State.isOk) {
          LOG_INFO("SENSOR-REC", F("All sensors recovered."));
          m_currentState = State::RUNNING;
          m_actionTimer.setInterval(AppConstants::SENSOR_INIT_RETRY_INTERVAL_MS);
        } else {
          // Stay in RECOVERY and keep retrying
          m_actionTimer.reset();
        }
      }
      break;
    }
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

void SensorManager::attemptSensorInitOrRecovery() {
  if (!m_shtState.isOk) {
    if (m_sht.init()) {
      m_sht.setAccuracy(SHTSensor::SHT_ACCURACY_MEDIUM);
      if (m_shtFailureNotified) {
        LOG_INFO("RECOVERY", F("SHT Sensor: RECOVERED"));
      }
      m_shtState.isOk = true;
      m_shtState.failureCount = 0;
      m_shtFailureNotified = false;
    } else {
      if (!m_shtFailureNotified) {
        LOG_ERROR("SENSOR", F("SHT Sensor: Init/Recovery failed. Will keep retrying silently."));
        m_shtFailureNotified = true;
      }
    }
  }

  if (!m_bh1750State.isOk) {
    if (m_lightMeter.begin()) {
      if (m_bh1750FailureNotified) {
        LOG_INFO("RECOVERY", F("BH1750: RECOVERED"));
      }
      m_bh1750State.isOk = true;
      m_bh1750State.failureCount = 0;
      m_bh1750FailureNotified = false;
      delay(AppConstants::BH1750_INIT_DELAY_MS);
    } else {
      if (!m_bh1750FailureNotified) {
        LOG_ERROR("SENSOR", F("BH1750: Init/Recovery failed. Will keep retrying silently."));
        m_bh1750FailureNotified = true;
      }
    }
  }
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
  LOG_INFO("I2C-REC", F("I2C bus re-initialized."));
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