#ifndef SET_PORTAL_PASS_COMMAND_H
#define SET_PORTAL_PASS_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class SetPortalPassCommand : REDACTED
public:
  explicit SetPortalPassCommand(ConfigManager& configManager);
  const char* getName() const override {
    return "REDACTED";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("setportalpass"); }
  const char* getDescription() const override {
    return "Sets WiFi portal password. Usage: REDACTED
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};
#endif