#ifndef CRASH_LOG_COMMAND_H
#define CRASH_LOG_COMMAND_H

#include "ICommand.h"

class CrashLogCommand : public ICommand {
public:
    PGM_P getName_P() const override { return PSTR("crashlog"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("crashlog"); }
    PGM_P getDescription_P() const override {
    return PSTR("Displays the system crash dump history.");
  }
  CommandSection helpSection() const override { return CommandSection::SYSTEM; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;
};

class ClearCrashCommand : public ICommand {
public:
    PGM_P getName_P() const override { return PSTR("clearcrash"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("clearcrash"); }
    PGM_P getDescription_P() const override {
    return PSTR("Deletes the crash dump history.");
  }
  CommandSection helpSection() const override { return CommandSection::SYSTEM; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;
};

#endif