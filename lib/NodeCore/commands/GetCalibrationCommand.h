#ifndef GET_CALIBRATION_COMMAND_H
#define GET_CALIBRATION_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class GetCalibrationCommand : public ICommand {
public:
  explicit GetCalibrationCommand(ConfigManager& configManager);
    PGM_P getName_P() const override { return PSTR("getcal"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("getcal"); }
    PGM_P getDescription_P() const override {
    return PSTR("Shows current sensor calibration values.");
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