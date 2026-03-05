#include "SensorManager.h"

#include <Wire.h>

#include "hardware_pins.h"
#include "Logger.h"

SensorManager::SensorManager()
    : m_lightMeter(AppConstants::BH1750_I2C_ADDR),
      m_currentState(State::INITIALIZING),
      m_shtReadTimer(AppConstants::SHT_READ_INTERVAL_MS),
      m_bh1750ReadTimer(AppConstants::BH1750_READ_INTERVAL_MS),
      m_actionTimer(AppConstants::SENSOR_INIT_RETRY_INTERVAL_MS),
      m_temperature(INVALID_TEMP),
      m_humidity(INVALID_HUMIDITY),
      m_lightLevel(INVALID_LUX),
      m_shtFailureNotified(false),
      m_bh1750FailureNotified(false) {}

void SensorManager::init() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000); // Enforce 100kHz standard mode

#ifdef ESP8266
  Wire.setClockStretchLimit(1000); // Timeout after ~1ms of clock stretching (prevents hanging)
#endif

  delay(AppConstants::I2C_SETTLE_DELAY_MS);

  m_currentState = State::INITIALIZING;
  m_actionTimer.reset();
  LOG_INFO("SENSOR", F("Initialization scheduled (non-blocking)."));
}

void SensorManager::pause() {
  if (m_currentState != State::PAUSED) {
    LOG_INFO("SENSOR", F("Pausing sensors for system stability."));
    m_currentState = State::PAUSED;
  }
}

void SensorManager::resume() {
  if (m_currentState == State::PAUSED) {
    LOG_INFO("SENSOR", F("Resuming sensors..."));
    m_currentState = State::INITIALIZING;
    m_actionTimer.reset();
  }
}

void SensorManager::handleImpl() {
  switch (m_currentState) {
    case State::INITIALIZING: handleInitializing(); break;
    case State::RUNNING:      handleRunning(); break;
    case State::RECOVERY:     handleRecovery(); break;
    case State::PAUSED:       break;
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
    m_recoveryFailureCount = 0;
    m_actionTimer.setInterval(AppConstants::SENSOR_INIT_RETRY_INTERVAL_MS);
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
    m_recoveryFailureCount = 0;
    m_actionTimer.setInterval(AppConstants::SENSOR_INIT_RETRY_INTERVAL_MS);
    m_actionTimer.reset();
  } else {
    m_recoveryFailureCount++;
    unsigned long nextInterval = AppConstants::SENSOR_INIT_RETRY_INTERVAL_MS;
    if (m_recoveryFailureCount >= 3) {
      nextInterval = AppConstants::SENSOR_SLOW_RETRY_INTERVAL_MS;
    }
    if (m_recoveryFailureCount >= 12) {
      nextInterval = AppConstants::SENSOR_RECOVERY_INTERVAL_MS;
    }
    if (m_actionTimer.getInterval() != nextInterval) {
      m_actionTimer.setInterval(nextInterval);
    }
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
  if (!m_bh1750State.isOk || !m_bh1750ReadTimer.hasElapsed()) {
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
  LOG_DEBUG("SENSOR", F("Initializing SHT..."));
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
  LOG_DEBUG("SENSOR", F("Initializing BH1750..."));
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

// Note: IRAM_ATTR removed because this function uses logging which
// requires Flash access. Safe to call from main loop context.
void SensorManager::recoverI2CBus() {
  LOG_WARN("I2C-REC", F("Attempting to recover I2C bus..."));
  auto releaseScl = []() { pinMode(PIN_I2C_SCL, INPUT_PULLUP); };
  auto pullSclLow = []() {
    pinMode(PIN_I2C_SCL, OUTPUT);
    digitalWrite(PIN_I2C_SCL, LOW);
  };
  auto releaseSda = []() { pinMode(PIN_I2C_SDA, INPUT_PULLUP); };
  auto pullSdaLow = []() {
    pinMode(PIN_I2C_SDA, OUTPUT);
    digitalWrite(PIN_I2C_SDA, LOW);
  };

  releaseSda();
  releaseScl();
  delayMicroseconds(8);

  bool sdaHigh = (digitalRead(PIN_I2C_SDA) == HIGH);
  bool sclHigh = (digitalRead(PIN_I2C_SCL) == HIGH);

  if (!sclHigh) {
    LOG_ERROR("I2C-REC", F("SCL stuck LOW. Check pull-up/device short."));
  }

  if (!sdaHigh && sclHigh) {
    LOG_WARN("I2C-REC", F("SDA stuck LOW. Clocking SCL to release slave..."));
    for (int i = 0; i < 18 && digitalRead(PIN_I2C_SDA) == LOW; ++i) {
      pullSclLow();
      delayMicroseconds(8);
      releaseScl();
      delayMicroseconds(8);
      yield();
    }
  }

  // Force a STOP condition to return the bus to idle.
  if (digitalRead(PIN_I2C_SCL) == HIGH) {
    pullSdaLow();
    delayMicroseconds(6);
    pullSclLow();
    delayMicroseconds(6);
    releaseScl();
    delayMicroseconds(8);
    releaseSda();
    delayMicroseconds(8);
  }

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
#ifdef ESP8266
  Wire.setClockStretchLimit(1000);
#endif
  delay(AppConstants::I2C_SETTLE_DELAY_MS);

  sdaHigh = (digitalRead(PIN_I2C_SDA) == HIGH);
  sclHigh = (digitalRead(PIN_I2C_SCL) == HIGH);

  // Prevent excessive logging by throttling I2C recovery messages.
  unsigned long now = millis();
  if ((!sdaHigh || !sclHigh) && (now - m_lastI2CLogTime > 10000)) {
    LOG_WARN("I2C-REC",
             F("Bus not idle after recovery (SDA=%u SCL=%u)"),
             sdaHigh ? 1 : 0,
             sclHigh ? 1 : 0);
    m_lastI2CLogTime = now;
  } else if (now - m_lastI2CLogTime > 60000) {
    LOG_INFO("I2C-REC",
             F("I2C bus re-initialized (SDA=%u SCL=%u)."),
             sdaHigh ? 1 : 0,
             sclHigh ? 1 : 0);
    m_lastI2CLogTime = now;
  }
}

SensorReading SensorManager::getTempImpl() const {
  bool isValid = m_shtState.isOk && (m_temperature != INVALID_TEMP);
  return {m_temperature, isValid};
}

SensorReading SensorManager::getHumidityImpl() const {
  bool isValid = m_shtState.isOk && (m_humidity != INVALID_HUMIDITY);
  return {m_humidity, isValid};
}

SensorReading SensorManager::getLightImpl() const {
  bool isValid = m_bh1750State.isOk && (m_lightLevel != INVALID_LUX);
  return {m_lightLevel, isValid};
}

bool SensorManager::getShtStatusImpl() const {
  return m_shtState.isOk;
}

bool SensorManager::getBh1750StatusImpl() const {
  return m_bh1750State.isOk;
}
