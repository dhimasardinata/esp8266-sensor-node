#ifndef SEND_NOW_COMMAND_H
#define SEND_NOW_COMMAND_H

#include "ICommand.h"

class ApiClient;

class SendNowCommand : public ICommand {
public:
  explicit SendNowCommand(ApiClient& apiClient);

  const char* getName() const override {
    return "send-now";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("send-now"); }

  const char* getDescription() const override {
    return "Creates a data record and schedules an immediate send attempt.";
  }

  bool requiresAuth() const override {
    return true;
  }

  void execute(const CommandContext& context) override;

private:
  ApiClient& m_apiClient;
};

#endif  // SEND_NOW_COMMAND_H