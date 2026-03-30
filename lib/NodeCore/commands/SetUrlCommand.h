#ifndef SET_URL_COMMAND_H
#define SET_URL_COMMAND_H

#include "ICommand.h"

class ConfigManager;

class SetUrlCommand : public ICommand {
public:
  explicit SetUrlCommand(ConfigManager& configManager);

  PGM_P getName_P() const override { return PSTR("seturl"); }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("seturl"); }
  PGM_P getDescription_P() const override {
    return PSTR("Set/show upload/OTA URL. Usage: REDACTED
  }
  CommandSection helpSection() const override { return CommandSection::CONFIGURATION; }
  bool requiresAuth() const override { return true; }

  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};

#endif
