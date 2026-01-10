#ifndef REBOOT_COMMAND_H
#define REBOOT_COMMAND_H

#include "ICommand.h"

class RebootCommand : public ICommand {
public:
  const char* getName() const override {
    return "reboot";
  }
  const char* getDescription() const override {
    return "Reboots the device.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;
};

#endif  // REBOOT_COMMAND_H