#ifndef SET_GATEWAY_COMMAND_H
#define SET_GATEWAY_COMMAND_H

#include "ICommand.h"

class ConfigManager;

class SetGatewayCommand : public ICommand {
public:
  explicit SetGatewayCommand(ConfigManager& configManager);

  PGM_P getName_P() const override { return PSTR("setgateway"); }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("setgateway"); }
  PGM_P getDescription_P() const override {
    return PSTR("Set/show local gateway host/IP. Usage: setgateway [gh1|gh2] [host|ip] <value|default|none|show>");
  }
  CommandSection helpSection() const override { return CommandSection::CONFIGURATION; }
  bool requiresAuth() const override { return true; }

  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};

#endif
