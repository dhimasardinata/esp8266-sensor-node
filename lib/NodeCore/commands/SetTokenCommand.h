#ifndef SET_TOKEN_COMMAND_H
#define SET_TOKEN_COMMAND_H

#include "ICommand.h"
class ConfigManager;

class SetTokenCommand : REDACTED
public:
  explicit SetTokenCommand(ConfigManager& configManager);

    PGM_P getName_P() const override { return PSTR("REDACTED"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("settoken"); }
    PGM_P getDescription_P() const override {
    return PSTR("Set/show upload/OTA token. Usage: REDACTED
  }
  CommandSection helpSection() const override { return CommandSection::CONFIGURATION; }
    bool requiresAuth() const override {
    return true;
  }

  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};

#endif  // SET_TOKEN_COMMAND_H
