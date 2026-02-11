#ifndef SYSINFO_COMMAND_H
#define SYSINFO_COMMAND_H

#include "ICommand.h"

// Minimal dependencies - no external services needed
class SysInfoCommand : public ICommand {
public:
    const char* getName() const override { return "sysinfo"; }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("sysinfo"); }
  const char* getDescription() const override { return "Shows hardware info (chip, flash, SDK)."; }
  bool requiresAuth() const override { return false; }
  void execute(const CommandContext& context) override;
};

#endif  // SYSINFO_COMMAND_H
