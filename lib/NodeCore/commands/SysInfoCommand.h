#ifndef SYSINFO_COMMAND_H
#define SYSINFO_COMMAND_H

#include "ICommand.h"

// Minimal dependencies - no external services needed
class SysInfoCommand : public ICommand {
public:
      PGM_P getName_P() const override { return PSTR("sysinfo"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("sysinfo"); }
    PGM_P getDescription_P() const override {
    return PSTR("Shows hardware info (chip, flash, SDK).");
  }
  CommandSection helpSection() const override { return CommandSection::PUBLIC; }
    bool requiresAuth() const override {
    return false;
  }
  void execute(const CommandContext& context) override;
};

#endif  // SYSINFO_COMMAND_H
