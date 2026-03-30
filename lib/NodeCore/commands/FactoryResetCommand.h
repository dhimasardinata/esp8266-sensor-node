#ifndef FACTORY_RESET_COMMAND_H
#define FACTORY_RESET_COMMAND_H

#include "ICommand.h"

class ConfigManager;
class CacheManager;  // Concrete type for CRTP

class FactoryResetCommand : public ICommand {
public:
  FactoryResetCommand(ConfigManager& configManager, CacheManager& cacheManager);
    PGM_P getName_P() const override { return PSTR("factoryreset"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("factoryreset"); }
    PGM_P getDescription_P() const override {
    return PSTR("WARNING: Deletes all configs and data, then reboots.");
  }
  CommandSection helpSection() const override { return CommandSection::SYSTEM; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
  CacheManager& m_cacheManager;
};

#endif