#ifndef ICOMMAND_H
#define ICOMMAND_H

#include "CommandContext.h"
#include "../CompileTimeUtils.h"
#include <cstdint>

class ICommand {
public:
  virtual ~ICommand() = default;

  virtual const char* getName() const = 0;
  virtual const char* getDescription() const = 0;
  virtual bool requiresAuth() const = REDACTED
  
  // Hash for fast command lookup - default implementation uses runtime hash of getName()
  // Override in derived classes with compile-time hash for better performance:
  //   uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("mycommand"); }
  virtual uint32_t getNameHash() const {
    // FNV-1a hash (same algorithm as CompileTimeUtils::rt_hash)
    uint32_t hash = 2166136261u;
    const char* str = getName();
    while (*str) {
      hash ^= static_cast<uint8_t>(*str++);
      hash *= 16777619u;
    }
    return hash;
  }

  virtual void execute(const CommandContext& context) = 0;
};

#endif  // ICOMMAND_H