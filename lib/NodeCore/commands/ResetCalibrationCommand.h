#ifndef RESET_CALIBRATION_COMMAND_H
#define RESET_CALIBRATION_COMMAND_H

#include "ICommand.h"

class ConfigManager;

class ResetCalibrationCommand : public ICommand {
public:
  explicit ResetCalibrationCommand(ConfigManager& configManager);
    PGM_P getName_P() const override { return PSTR("resetcal"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("resetcal"); }
    PGM_P getDescription_P() const override {
    return PSTR("Resets calibration values to firmware defaults.");
  }
  CommandSection helpSection() const override { return CommandSection::CALIBRATION; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};
#endif