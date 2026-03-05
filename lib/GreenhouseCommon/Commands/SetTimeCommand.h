#ifndef SET_TIME_COMMAND_H
#define SET_TIME_COMMAND_H

#include "ICommand.h"

class NtpClient;

class SetTimeCommand : public ICommand {
public:
  explicit SetTimeCommand(NtpClient& ntpClient) : m_ntpClient(ntpClient) {}
  const char* getName() const override {
    return "settime";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("settime"); }
  const char* getDescription() const override {
    return "Set local time. Usage: settime <epoch> OR settime YYYY-MM-DD HH:MM:SS";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  NtpClient& m_ntpClient;
};

#endif
