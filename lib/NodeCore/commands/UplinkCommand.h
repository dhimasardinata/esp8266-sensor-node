#ifndef UPLINK_COMMAND_H
#define UPLINK_COMMAND_H

#include "ICommand.h"

class ApiClient;
class ConfigManager;

class UplinkCommand : public ICommand {
public:
  UplinkCommand(ConfigManager& configManager, ApiClient& apiClient)
      : m_configManager(configManager), m_apiClient(apiClient) {}

  PGM_P getName_P() const override { return PSTR("uplink"); }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("uplink"); }
  PGM_P getDescription_P() const override {
    return PSTR("Show/set cloud uplink path. Usage: uplink [show|auto|direct|relay]");
  }
  CommandSection helpSection() const override { return CommandSection::CONFIGURATION; }
  bool requiresAuth() const override { return true; }

  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
  ApiClient& m_apiClient;
};

#endif  // UPLINK_COMMAND_H
