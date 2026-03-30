#ifndef FORMAT_FS_COMMAND_H
#define FORMAT_FS_COMMAND_H

#include "ICommand.h"

class ConfigManager;

class FormatFsCommand : public ICommand {
public:
  explicit FormatFsCommand(ConfigManager& configManager);
    PGM_P getName_P() const override { return PSTR("format"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("format"); }
    PGM_P getDescription_P() const override {
    return PSTR("WARNING: Formats the filesystem, deleting ALL files.");
  }
  CommandSection helpSection() const override { return CommandSection::SYSTEM; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};

#endif