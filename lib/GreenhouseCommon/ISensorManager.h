#ifndef I_SENSOR_MANAGER_H
#define I_SENSOR_MANAGER_H

#include "sensor_data.h"

// ============================================================================
// CRTP Base Class for Zero-Overhead Sensor Interface
// ============================================================================
// 
// CRTP (Curiously Recurring Template Pattern) provides compile-time
// polymorphism without virtual function overhead:
// - No vtable pointer (saves 4 bytes per object)
// - No indirect function calls (enables inlining)
// - Branch predictor works better with direct calls
//
// Usage:
//   class SensorManager : public ISensorManager<SensorManager> { ... };

template <typename Derived>
class ISensorManager {
public:
  // No virtual destructor needed - CRTP uses static dispatch
  
  void handle() {
    static_cast<Derived*>(this)->handleImpl();
  }
  
  SensorReading getTemp() const {
    return static_cast<const Derived*>(this)->getTempImpl();
  }
  
  SensorReading getHumidity() const {
    return static_cast<const Derived*>(this)->getHumidityImpl();
  }
  
  SensorReading getLight() const {
    return static_cast<const Derived*>(this)->getLightImpl();
  }
  
  bool getShtStatus() const {
    return static_cast<const Derived*>(this)->getShtStatusImpl();
  }
  
  bool getBh1750Status() const {
    return static_cast<const Derived*>(this)->getBh1750StatusImpl();
  }

protected:
  // Protected constructor prevents direct instantiation
  ISensorManager() = default;
  ~ISensorManager() = default;
  
  // Prevent copying/moving of base class
  ISensorManager(const ISensorManager&) = default;
  ISensorManager& operator=(const ISensorManager&) = default;
};

// NOTE: ISensorManagerVirtual has been removed.
// All code now uses CRTP for zero-overhead polymorphism.

#endif  // I_SENSOR_MANAGER_H