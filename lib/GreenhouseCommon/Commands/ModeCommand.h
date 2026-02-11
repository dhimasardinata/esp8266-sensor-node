#ifndef MODE_COMMAND_H
#define MODE_COMMAND_H

#include "ICommand.h"

class ApiClient;

class ModeCommand : public ICommand {
public:
  explicit ModeCommand(ApiClient& apiClient) : m_apiClient(apiClient) {}

  const char* getName() const override { return "mode"; }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("mode"); }
  const char* getDescription() const override {
    return "Get/set upload mode: mode [auto|cloud|edge]";
  }
  bool requiresAuth() const override { return true; }

  void execute(const CommandContext& context) override;

private:
  ApiClient& m_apiClient;
};

#endif  // MODE_COMMAND_H
