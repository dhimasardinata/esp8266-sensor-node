#ifndef MODE_COMMAND_H
#define MODE_COMMAND_H

#include "ICommand.h"

class ApiClient;

class ModeCommand : public ICommand {
public:
  explicit ModeCommand(ApiClient& apiClient) : m_apiClient(apiClient) {}

    PGM_P getName_P() const override { return PSTR("mode"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("mode"); }
    PGM_P getDescription_P() const override {
    return PSTR("Show/set upload mode. Usage: mode [show|auto|cloud|edge]");
  }
  CommandSection helpSection() const override { return CommandSection::SYSTEM; }
    bool requiresAuth() const override {
    return true;
  }

  void execute(const CommandContext& context) override;

private:
  ApiClient& m_apiClient;
};

#endif  // MODE_COMMAND_H
