#ifndef SET_PORTAL_PASS_COMMAND_H
#define SET_PORTAL_PASS_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class SetPortalPassCommand : REDACTED
public:
  explicit SetPortalPassCommand(ConfigManager& configManager);
    PGM_P getName_P() const override { return PSTR("REDACTED"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("setportalpass"); }
    PGM_P getDescription_P() const override {
    return PSTR("Sets WiFi portal password. Usage: REDACTED
  }
  CommandSection helpSection() const override { return CommandSection::CONFIGURATION; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};
#endif