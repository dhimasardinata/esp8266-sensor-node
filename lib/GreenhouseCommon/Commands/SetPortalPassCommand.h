#ifndef SET_PORTAL_PASS_COMMAND_H
#define SET_PORTAL_PASS_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class SetPortalPassCommand : public ICommand {
public:
  explicit SetPortalPassCommand(ConfigManager& configManager);
  const char* getName() const override {
    return "setportalpass";
  }
  const char* getDescription() const override {
    return "Sets WiFi portal password. Usage: setportalpass <pass>";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};
#endif