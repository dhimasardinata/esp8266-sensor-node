#ifndef READ_SENSORS_COMMAND_H
#define READ_SENSORS_COMMAND_H

#include "ICommand.h"

// Forward declare dependencies to keep header clean
class SensorManager;  // Concrete type for CRTP
class ConfigManager;

class ReadSensorsCommand : public ICommand {
public:
  // --- MODIFICATION: Depend on the interface, not the concrete class ---
  ReadSensorsCommand(SensorManager& sensorManager, ConfigManager& configManager);

  const char* getName() const override {
    return "read-sensors";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("read-sensors"); }
  const char* getDescription() const override {
    return "Reads and displays current sensor values (raw and calibrated).";
  }
  bool requiresAuth() const override {
    return false;
  }
  void execute(const CommandContext& context) override;

private:
  // --- MODIFICATION: Store a reference to the interface ---
  SensorManager& m_sensorManager;
  ConfigManager& m_configManager;
};

#endif  // READ_SENSORS_COMMAND_H