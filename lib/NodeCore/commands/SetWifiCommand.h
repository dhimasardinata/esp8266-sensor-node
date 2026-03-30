#ifndef SET_WIFI_COMMAND_H
#define SET_WIFI_COMMAND_H

#include "ICommand.h"

class ConfigManager;

class SetWifiCommand : REDACTED
public:
  explicit SetWifiCommand(ConfigManager& configManager);

    PGM_P getName_P() const override { return PSTR("REDACTED"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("setwifi"); }
    PGM_P getDescription_P() const override {
    return PSTR("Sets new WiFi credentials and reboots. Usage: REDACTED
  }
  CommandSection helpSection() const override { return CommandSection::CONFIGURATION; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};

#endif  // SET_WIFI_COMMAND_H