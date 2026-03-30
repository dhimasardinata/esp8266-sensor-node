#ifndef LOGIN_COMMAND_H
#define LOGIN_COMMAND_H

#include "ICommand.h"

class ConfigManager;
class DiagnosticsTerminal;  // Concrete type for CRTP

class LoginCommand : public ICommand {
public:
  LoginCommand(ConfigManager& configManager, DiagnosticsTerminal& authManager);

    PGM_P getName_P() const override { return PSTR("login"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("login"); }
    PGM_P getDescription_P() const override {
    return PSTR("Authenticate to use protected commands. Usage: REDACTED
  }
  CommandSection helpSection() const override { return CommandSection::PUBLIC; }
    bool requiresAuth() const override {
    return false;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
  DiagnosticsTerminal& m_authManager;
};

#endif  // LOGIN_COMMAND_H