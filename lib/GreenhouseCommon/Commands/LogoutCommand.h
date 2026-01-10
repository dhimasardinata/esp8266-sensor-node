#ifndef LOGOUT_COMMAND_H
#define LOGOUT_COMMAND_H

#include "ICommand.h"

class IAuthManager;

class LogoutCommand : public ICommand {
public:
  explicit LogoutCommand(IAuthManager& authManager);
  
  const char* getName() const override {
    return "logout";
  }
  const char* getDescription() const override {
    return "De-authenticates the current session.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  IAuthManager& m_authManager;
};

#endif  // LOGOUT_COMMAND_H