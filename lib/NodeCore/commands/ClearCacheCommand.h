#ifndef CLEAR_CACHE_COMMAND_H
#define CLEAR_CACHE_COMMAND_H

#include "ICommand.h"

class CacheManager;  // Concrete type for CRTP

class ClearCacheCommand : public ICommand {
public:
  explicit ClearCacheCommand(CacheManager& cacheManager);

    PGM_P getName_P() const override { return PSTR("clearcache"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("clearcache"); }
    PGM_P getDescription_P() const override {
    return PSTR("Clears the sensor data cache.");
  }
  CommandSection helpSection() const override { return CommandSection::SENSORS_DATA; }
    bool requiresAuth() const override {
    return true;
  }

  void execute(const CommandContext& context) override;

private:
  CacheManager& m_cacheManager;
};

#endif  // CLEAR_CACHE_COMMAND_H