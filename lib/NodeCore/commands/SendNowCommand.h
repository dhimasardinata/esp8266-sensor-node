#ifndef SEND_NOW_COMMAND_H
#define SEND_NOW_COMMAND_H

#include "ICommand.h"

class ApiClient;

class SendNowCommand : public ICommand {
public:
  explicit SendNowCommand(ApiClient& apiClient);

    PGM_P getName_P() const override { return PSTR("sendnow"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("sendnow"); }

    PGM_P getDescription_P() const override {
    return PSTR("Creates a data record and schedules an immediate send attempt.");
  }
  CommandSection helpSection() const override { return CommandSection::SENSORS_DATA; }

    bool requiresAuth() const override {
    return true;
  }

  void execute(const CommandContext& context) override;

private:
  ApiClient& m_apiClient;
};

#endif  // SEND_NOW_COMMAND_H