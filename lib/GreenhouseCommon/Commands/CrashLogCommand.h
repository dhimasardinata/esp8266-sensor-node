#ifndef CRASH_LOG_COMMAND_H
#define CRASH_LOG_COMMAND_H

#include "ICommand.h"

class CrashLogCommand : public ICommand {
public:
  const char* getName() const override {
    return "crashlog";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("crashlog"); }
  const char* getDescription() const override {
    return "Displays the system crash dump history.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;
};

class ClearCrashCommand : public ICommand {
public:
  const char* getName() const override {
    return "clearcrash";
  }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("clearcrash"); }
  const char* getDescription() const override {
    return "Deletes the crash dump history.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;
};

#endif