#ifndef GET_CONFIG_COMMAND_H
#define GET_CONFIG_COMMAND_H

#include "ICommand.h"

class ConfigManager;

class GetConfigCommand : public ICommand {
public:
  explicit GetConfigCommand(ConfigManager& configManager);

  const char* getName() const override {
    return "get-config";
  }
  const char* getDescription() const override {
    return "Prints the current application configuration.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};

#endif  // GET_CONFIG_COMMAND_H