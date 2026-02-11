#ifndef FACTORY_RESET_COMMAND_H
#define FACTORY_RESET_COMMAND_H

#include "ICommand.h"

class ConfigManager;
class CacheManager;  // Concrete type for CRTP

class FactoryResetCommand : public ICommand {
public:
  FactoryResetCommand(ConfigManager& configManager, CacheManager& cacheManager);
  const char* getName() const override {
    return "factory-reset";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("factory-reset"); }
  const char* getDescription() const override {
    return "WARNING: Deletes all configs and data, then reboots.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
  CacheManager& m_cacheManager;
};

#endif