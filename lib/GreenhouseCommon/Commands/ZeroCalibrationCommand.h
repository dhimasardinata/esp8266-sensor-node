#ifndef ZERO_CALIBRATION_COMMAND_H
#define ZERO_CALIBRATION_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class ZeroCalibrationCommand : public ICommand {
public:
  explicit ZeroCalibrationCommand(ConfigManager& configManager);
  const char* getName() const override {
    return "zerocal";
  }
  const char* getDescription() const override {
    return "Resets calibration to neutral (offsets 0.0, factor 1.0).";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};
#endif