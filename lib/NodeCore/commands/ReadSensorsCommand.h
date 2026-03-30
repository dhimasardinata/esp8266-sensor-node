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

    PGM_P getName_P() const override { return PSTR("read"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("read"); }
    PGM_P getDescription_P() const override {
    return PSTR("Reads and displays current sensor values (raw and calibrated).");
  }
  CommandSection helpSection() const override { return CommandSection::SENSORS_DATA; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  // --- MODIFICATION: Store a reference to the interface ---
  SensorManager& m_sensorManager;
  ConfigManager& m_configManager;
};

#endif  // READ_SENSORS_COMMAND_H