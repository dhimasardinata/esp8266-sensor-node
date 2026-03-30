#ifndef ICOMMAND_H
#define ICOMMAND_H

#include "CommandContext.h"
#include "support/CompileTimeUtils.h"
#include <cstdint>

enum class CommandSection : uint8_t {
  PUBLIC,
  SENSORS_DATA,
  CALIBRATION,
  CONFIGURATION,
  WIFI,
  SYSTEM
};

class ICommand {
public:
  virtual ~ICommand() = default;

  virtual PGM_P getName_P() const = 0;
  virtual PGM_P getDescription_P() const = 0;
  virtual CommandSection helpSection() const = 0;
  virtual bool requiresAuth() const = REDACTED

  const char* getName() const { return reinterpret_cast<const char*>(getName_P()); }
  const char* getDescription() const { return reinterpret_cast<const char*>(getDescription_P()); }
  
  // Hash for fast command lookup - default implementation uses runtime hash of getName_P()
  // Override in derived classes with compile-time hash for better performance:
  //   uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("mycommand"); }
  virtual uint32_t getNameHash() const {
    // FNV-1a hash (same algorithm as CompileTimeUtils::rt_hash)
    uint32_t hash = 2166136261u;
    const char* str = reinterpret_cast<const char*>(getName_P());
    while (*str) {
      hash ^= static_cast<uint8_t>(*str++);
      hash *= 16777619u;
    }
    return hash;
  }

  virtual void execute(const CommandContext& context) = 0;
};

#endif  // ICOMMAND_H
