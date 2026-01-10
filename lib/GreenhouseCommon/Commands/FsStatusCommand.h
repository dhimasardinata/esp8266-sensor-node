#ifndef FS_STATUS_COMMAND_H
#define FS_STATUS_COMMAND_H

#include "ICommand.h"

class FsStatusCommand : public ICommand {
public:
  const char* getName() const override {
    return "fs_status";
  }
  const char* getDescription() const override {
    return "Shows filesystem (LittleFS) usage statistics.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;
};

#endif  // FS_STATUS_COMMAND_H