#ifndef CLEAR_CACHE_COMMAND_H
#define CLEAR_CACHE_COMMAND_H

#include "ICommand.h"

class CacheManager;  // Concrete type for CRTP

class ClearCacheCommand : public ICommand {
public:
  explicit ClearCacheCommand(CacheManager& cacheManager);

  const char* getName() const override { return "clear-cache"; }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("clear-cache"); }
  const char* getDescription() const override {
    return "Clears the sensor data cache.";
  }
  bool requiresAuth() const override { return true; }

  void execute(const CommandContext& context) override;

private:
  CacheManager& m_cacheManager;
};

#endif  // CLEAR_CACHE_COMMAND_H