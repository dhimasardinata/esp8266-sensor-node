#ifndef ZERO_CALIBRATION_COMMAND_H
#define ZERO_CALIBRATION_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class ZeroCalibrationCommand : public ICommand {
public:
  explicit ZeroCalibrationCommand(ConfigManager& configManager);
    PGM_P getName_P() const override { return PSTR("zerocal"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("zerocal"); }
    PGM_P getDescription_P() const override {
    return PSTR("Resets calibration to neutral (offsets 0.0, factor 1.0).");
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