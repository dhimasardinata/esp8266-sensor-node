#ifndef FS_STATUS_COMMAND_H
#define FS_STATUS_COMMAND_H

#include "ICommand.h"

class FsStatusCommand : public ICommand {
public:
    PGM_P getName_P() const override { return PSTR("fsstatus"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("fsstatus"); }
    PGM_P getDescription_P() const override {
    return PSTR("Shows filesystem (LittleFS) usage statistics.");
  }
  CommandSection helpSection() const override { return CommandSection::SYSTEM; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;
};

#endif  // FS_STATUS_COMMAND_H