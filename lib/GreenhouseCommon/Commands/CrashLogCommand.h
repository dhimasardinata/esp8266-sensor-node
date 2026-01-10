#ifndef CRASH_LOG_COMMAND_H
#define CRASH_LOG_COMMAND_H

#include "ICommand.h"

class CrashLogCommand : public ICommand {
public:
  const char* getName() const override {
    return "crashlog";
  }
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
  const char* getDescription() const override {
    return "Deletes the crash dump history.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;
};

#endif