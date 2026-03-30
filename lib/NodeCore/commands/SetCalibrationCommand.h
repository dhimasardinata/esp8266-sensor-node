#ifndef SET_CALIBRATION_COMMAND_H
#define SET_CALIBRATION_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class SetCalibrationCommand : public ICommand {
public:
  explicit SetCalibrationCommand(ConfigManager& configManager);
    PGM_P getName_P() const override { return PSTR("setcal"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("setcal"); }
    PGM_P getDescription_P() const override {
    return PSTR("Sets calibration. Usage: setcal <temp> <hum> <lux>");
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