#ifndef CLEAR_CACHE_COMMAND_H
#define CLEAR_CACHE_COMMAND_H

#include "ICacheManager.h"  // <-- Include the interface
#include "ICommand.h"

class ClearCacheCommand : public ICommand {
public:
  // --- MODIFICATION: Constructor now requires an ICacheManager ---
  explicit ClearCacheCommand(ICacheManager& cacheManager);

  const char* getName() const override {
    return "clear-cache";
  }
  const char* getDescription() const override {
    return "Deletes all records from the sensor data cache.";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  // --- MODIFICATION: Store a reference to the cache manager ---
  ICacheManager& m_cacheManager;
};

#endif