#ifndef RESET_CALIBRATION_COMMAND_H
#define RESET_CALIBRATION_COMMAND_H

#include "ICommand.h"

class ConfigManager;

class ResetCalibrationCommand : public ICommand {
public:
  explicit ResetCalibrationCommand(ConfigManager& configManager);
  const char* getName() const override {
    return "reset-cal";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("reset-cal"); }
  const char* getDescription() const override {
    return "Resets calibration values to firmware defaults.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};
#endif