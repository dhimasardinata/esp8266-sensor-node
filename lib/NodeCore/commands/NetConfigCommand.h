#ifndef NET_CONFIG_COMMAND_H
#define NET_CONFIG_COMMAND_H

#include "ICommand.h"

class ApiClient;
class ConfigManager;

class NetConfigCommand : public ICommand {
public:
  NetConfigCommand(ConfigManager& configManager, ApiClient& apiClient);

  PGM_P getName_P() const override { return PSTR("netconfig"); }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("netconfig"); }
  PGM_P getDescription_P() const override {
    return PSTR("Show network config summary. Usage: netconfig [show]");
  }
  CommandSection helpSection() const override { return CommandSection::CONFIGURATION; }
  bool requiresAuth() const override { return true; }

  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
  ApiClient& m_apiClient;
};

#endif
