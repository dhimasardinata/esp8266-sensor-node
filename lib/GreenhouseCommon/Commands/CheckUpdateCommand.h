#ifndef CHECK_UPDATE_COMMAND_H
#define CHECK_UPDATE_COMMAND_H

#include "ICommand.h"

class OtaManager;
class WifiManager;
class NtpClient;

class CheckUpdateCommand : public ICommand {
public:
  CheckUpdateCommand(OtaManager& otaManager, WifiManager& wifiManager, NtpClient& ntpClient);
  const char* getName() const override {
    return "check-update";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("check-update"); }
  const char* getDescription() const override {
    return "Forces a check for new firmware updates.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  OtaManager& m_otaManager;
  WifiManager& m_wifiManager;
  NtpClient& m_ntpClient;
};

#endif