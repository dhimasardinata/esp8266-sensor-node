#ifndef MOCK_SENSOR_MANAGER_H
#define MOCK_SENSOR_MANAGER_H

#include <ISensorManager.h>

class MockSensorManager : public ISensorManager {
public:
  // --- Control variables for the test ---
  SensorReading mockTemp = {0.0f, false};
  SensorReading mockHum = {0.0f, false};
  SensorReading mockLight = {0.0f, false};
  bool mockShtStatus = false;
  bool mockBh1750Status = false;
  int handle_call_count = 0;

  // --- Interface implementation ---
  void handle() override {
    // We can track if this was called
    handle_call_count++;
  }

  SensorReading getTemp() const override {
    return mockTemp;
  }
  SensorReading getHumidity() const override {
    return mockHum;
  }
  SensorReading getLight() const override {
    return mockLight;
  }
  bool getShtStatus() const override {
    return mockShtStatus;
  }
  bool getBh1750Status() const override {
    return mockBh1750Status;
  }
};

#endif  // MOCK_SENSOR_MANAGER_H