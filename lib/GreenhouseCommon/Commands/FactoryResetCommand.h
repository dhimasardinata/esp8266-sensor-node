#ifndef FACTORY_RESET_COMMAND_H
#define FACTORY_RESET_COMMAND_H

#include "ICommand.h"

class ConfigManager;
class ICacheManager;

class FactoryResetCommand : public ICommand {
public:
  FactoryResetCommand(ConfigManager& configManager, ICacheManager& cacheManager);
  const char* getName() const override {
    return "factory-reset";
  }
  const char* getDescription() const override {
    return "WARNING: Deletes all configs and data, then reboots.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
  ICacheManager& m_cacheManager;
};

#endif