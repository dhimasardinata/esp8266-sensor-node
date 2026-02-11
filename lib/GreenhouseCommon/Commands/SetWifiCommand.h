#ifndef SET_WIFI_COMMAND_H
#define SET_WIFI_COMMAND_H

#include "ICommand.h"

class ConfigManager;

class SetWifiCommand : REDACTED
public:
  explicit SetWifiCommand(ConfigManager& configManager);

  const char* getName() const override {
    return "REDACTED";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("setwifi"); }
  const char* getDescription() const override {
    return "Sets new WiFi credentials and reboots. Usage: REDACTED
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};

#endif  // SET_WIFI_COMMAND_H