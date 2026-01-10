#ifndef HELP_COMMAND_H
#define HELP_COMMAND_H

#include <map>
#include <memory>
#include <string>  // Ditambahkan untuk std::string
#include <vector> // Ditambahkan untuk std::vector

#include "ICommand.h"

class HelpCommand : public ICommand {
public:
  // Updated to use vector
  explicit HelpCommand(const std::vector<std::unique_ptr<ICommand>>& commands);

  const char* getName() const override {
    return "help";
  }
  const char* getDescription() const override {
    return "Shows this list of available commands.";
  }
  bool requiresAuth() const override {
    return false;
  }

  // Sekarang hanya ada satu metode execute yang meng-override ICommand
  void execute(const CommandContext& context) override;

private:
  const std::vector<std::unique_ptr<ICommand>>& m_commands;
};

#endif  // HELP_COMMAND_H