#ifndef SET_CONFIG_COMMAND_H
#define SET_CONFIG_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class SetConfigCommand : public ICommand {
public:
  SetConfigCommand(ConfigManager& configManager);
    PGM_P getName_P() const override { return PSTR("setconfig"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("setconfig"); }
    PGM_P getDescription_P() const override {
    return PSTR("Sets timing config. Usage: setconfig <key> <value>");
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