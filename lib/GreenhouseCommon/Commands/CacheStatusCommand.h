#include "ICommand.h"

class CacheManager;  // Concrete type for CRTP

class CacheStatusCommand : public ICommand {
public:
  explicit CacheStatusCommand(CacheManager& cacheManager);

  const char* getName() const override { return "cache-status"; }
  uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("cache-status"); }
  const char* getDescription() const override {
    return "Displays the current cache status.";
  }
  bool requiresAuth() const override { return false; }

  void execute(const CommandContext& context) override;

private:
  CacheManager& m_cacheManager;
};