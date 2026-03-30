#include "ICommand.h"

class CacheManager;  // Concrete type for CRTP
class ApiClient;
class ConfigManager;

class CacheStatusCommand : public ICommand {
public:
  CacheStatusCommand(CacheManager& cacheManager, ApiClient& apiClient, ConfigManager& configManager);

    PGM_P getName_P() const override { return PSTR("cache"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("cache"); }
    PGM_P getDescription_P() const override {
    return PSTR("Displays cache status and hold-time estimates.");
  }
  CommandSection helpSection() const override { return CommandSection::SENSORS_DATA; }
    bool requiresAuth() const override {
    return true;
  }

  void execute(const CommandContext& context) override;

private:
  CacheManager& m_cacheManager;
  ApiClient& m_apiClient;
  ConfigManager& m_configManager;
};
