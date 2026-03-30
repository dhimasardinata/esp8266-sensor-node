#ifndef SET_TIME_COMMAND_H
#define SET_TIME_COMMAND_H

#include "ICommand.h"

class NtpClient;

class SetTimeCommand : public ICommand {
public:
  explicit SetTimeCommand(NtpClient& ntpClient) : m_ntpClient(ntpClient) {}
    PGM_P getName_P() const override { return PSTR("settime"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("settime"); }
    PGM_P getDescription_P() const override {
    return PSTR("Set local time. Usage: settime <epoch> OR settime YYYY-MM-DD HH:MM:SS");
  }
  CommandSection helpSection() const override { return CommandSection::CONFIGURATION; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  NtpClient& m_ntpClient;
};

#endif
