#ifndef GET_CONFIG_COMMAND_H
#define GET_CONFIG_COMMAND_H

#include "ICommand.h"

class ConfigManager;

class GetConfigCommand : public ICommand {
public:
  explicit GetConfigCommand(ConfigManager& configManager);

    PGM_P getName_P() const override { return PSTR("getconfig"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("getconfig"); }
    PGM_P getDescription_P() const override {
    return PSTR("REDACTED");
  }
  CommandSection helpSection() const override { return CommandSection::CONFIGURATION; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};

#endif  // GET_CONFIG_COMMAND_H
