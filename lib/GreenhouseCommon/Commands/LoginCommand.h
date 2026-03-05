#ifndef LOGIN_COMMAND_H
#define LOGIN_COMMAND_H

#include "ICommand.h"

class ConfigManager;
class DiagnosticsTerminal;  // Concrete type for CRTP

class LoginCommand : public ICommand {
public:
  LoginCommand(ConfigManager& configManager, DiagnosticsTerminal& authManager);

  const char* getName() const override {
    return "login";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("login"); }
  const char* getDescription() const override {
    return "Authenticate to use protected commands. Usage: REDACTED
  }
  bool requiresAuth() const override {
    return false;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
  DiagnosticsTerminal& m_authManager;
};

#endif  // LOGIN_COMMAND_H