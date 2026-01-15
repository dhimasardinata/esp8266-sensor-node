#ifndef SET_CONFIG_COMMAND_H
#define SET_CONFIG_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class SetConfigCommand : public ICommand {
public:
  SetConfigCommand(ConfigManager& configManager);
  const char* getName() const override {
    return "setconfig";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("setconfig"); }
  const char* getDescription() const override {
    return "Sets timing config. Usage: setconfig <key> <value>";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};
#endif