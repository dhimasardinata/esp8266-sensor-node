#ifndef LOGOUT_COMMAND_H
#define LOGOUT_COMMAND_H

#include "ICommand.h"

class DiagnosticsTerminal;  // Concrete type for CRTP

class LogoutCommand : public ICommand {
public:
  explicit LogoutCommand(DiagnosticsTerminal& authManager);
  
    PGM_P getName_P() const override { return PSTR("logout"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("logout"); }
    PGM_P getDescription_P() const override {
    return PSTR("REDACTED");
  }
  CommandSection helpSection() const override { return CommandSection::PUBLIC; }
    bool requiresAuth() const override {
    return false;
  }
  void execute(const CommandContext& context) override;

private:
  DiagnosticsTerminal& m_authManager;
};

#endif  // LOGOUT_COMMAND_H