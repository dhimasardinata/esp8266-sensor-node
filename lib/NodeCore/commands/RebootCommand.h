#ifndef REBOOT_COMMAND_H
#define REBOOT_COMMAND_H

#include "ICommand.h"

class RebootCommand : public ICommand {
public:
    PGM_P getName_P() const override { return PSTR("reboot"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("reboot"); }
    PGM_P getDescription_P() const override {
    return PSTR("Reboots the device.");
  }
  CommandSection helpSection() const override { return CommandSection::SYSTEM; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;
};

#endif  // REBOOT_COMMAND_H