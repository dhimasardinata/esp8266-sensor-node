#ifndef GET_CALIBRATION_COMMAND_H
#define GET_CALIBRATION_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class GetCalibrationCommand : public ICommand {
public:
  explicit GetCalibrationCommand(ConfigManager& configManager);
  const char* getName() const override {
    return "getcal";
  }
  const char* getDescription() const override {
    return "Shows current sensor calibration values.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};
#endif