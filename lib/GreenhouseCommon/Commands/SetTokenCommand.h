#ifndef SET_TOKEN_COMMAND_H
#define SET_TOKEN_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class SetTokenCommand : public ICommand {
public:
  explicit SetTokenCommand(ConfigManager& configManager);

  const char* getName() const override {
    return "settoken";
  }
  const char* getDescription() const override {
    return "Sets API auth token. Usage: settoken <token>";
  }
  bool requiresAuth() const override {
    return true;
  }

  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};

#endif  // SET_TOKEN_COMMAND_H