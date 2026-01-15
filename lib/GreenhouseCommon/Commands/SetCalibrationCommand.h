#ifndef SET_CALIBRATION_COMMAND_H
#define SET_CALIBRATION_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class SetCalibrationCommand : public ICommand {
public:
  explicit SetCalibrationCommand(ConfigManager& configManager);
  const char* getName() const override {
    return "setcal";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("setcal"); }
  const char* getDescription() const override {
    return "Sets calibration. Usage: setcal <temp> <hum> <lux>";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};
#endif