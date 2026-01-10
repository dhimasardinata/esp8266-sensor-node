#ifndef LOGIN_COMMAND_H
#define LOGIN_COMMAND_H

#include "ICommand.h"

class ConfigManager;
class IAuthManager;

class LoginCommand : public ICommand {
public:
  LoginCommand(ConfigManager& configManager, IAuthManager& authManager);

  const char* getName() const override {
    return "login";
  }
  const char* getDescription() const override {
    return "Authenticate to use protected commands. Usage: login <password>";
  }
  bool requiresAuth() const override {
    return false;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
  IAuthManager& m_authManager;
};

#endif  // LOGIN_COMMAND_H