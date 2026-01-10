#ifndef FORMAT_FS_COMMAND_H
#define FORMAT_FS_COMMAND_H

#include "ICommand.h"

class ConfigManager;

class FormatFsCommand : public ICommand {
public:
  explicit FormatFsCommand(ConfigManager& configManager);
  const char* getName() const override {
    return "format-fs";
  }
  const char* getDescription() const override {
    return "WARNING: Formats the filesystem, deleting ALL files.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ConfigManager& m_configManager;
};

#endif