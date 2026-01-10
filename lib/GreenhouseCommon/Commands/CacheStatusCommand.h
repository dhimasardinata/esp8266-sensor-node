#ifndef CACHE_STATUS_COMMAND_H
#define CACHE_STATUS_COMMAND_H

#include "ICacheManager.h"  // <-- Include the interface
#include "ICommand.h"

class CacheStatusCommand : public ICommand {
public:
  // --- MODIFICATION: Constructor now requires an ICacheManager ---
  explicit CacheStatusCommand(ICacheManager& cacheManager);

  const char* getName() const override {
    return "cache-status";
  }
  const char* getDescription() const override {
    return "Shows the current cache status (size, head, tail).";
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