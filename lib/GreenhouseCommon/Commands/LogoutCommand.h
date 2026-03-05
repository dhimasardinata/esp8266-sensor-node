#ifndef LOGOUT_COMMAND_H
#define LOGOUT_COMMAND_H

#include "ICommand.h"

class DiagnosticsTerminal;  // Concrete type for CRTP

class LogoutCommand : public ICommand {
public:
  explicit LogoutCommand(DiagnosticsTerminal& authManager);
  
  const char* getName() const override {
    return "logout";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("logout"); }
  const char* getDescription() const override {
    return "REDACTED";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  DiagnosticsTerminal& m_authManager;
};

#endif  // LOGOUT_COMMAND_H