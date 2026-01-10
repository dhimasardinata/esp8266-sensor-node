#ifndef ICOMMAND_H
#define ICOMMAND_H

#include "CommandContext.h"  // Sertakan definisi context yang baru

class ICommand {
public:
  virtual ~ICommand() = default;

  virtual const char* getName() const = 0;
  virtual const char* getDescription() const = 0;
  virtual bool requiresAuth() const = 0;

  // Tanda tangan fungsi yang baru
  virtual void execute(const CommandContext& context) = 0;
};

#endif  // ICOMMAND_H