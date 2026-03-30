#ifndef CHECK_UPDATE_COMMAND_H
#define CHECK_UPDATE_COMMAND_H

#include "ICommand.h"

class OtaManager;
class WifiManager;
class NtpClient;

class CheckUpdateCommand : public ICommand {
public:
  CheckUpdateCommand(OtaManager& otaManager, WifiManager& wifiManager, NtpClient& ntpClient);
    PGM_P getName_P() const override { return PSTR("checkupdate"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("checkupdate"); }
    PGM_P getDescription_P() const override {
    return PSTR("Forces a check for new firmware updates.");
  }
  CommandSection helpSection() const override { return CommandSection::SYSTEM; }
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