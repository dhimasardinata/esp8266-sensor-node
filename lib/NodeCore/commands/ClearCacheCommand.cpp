#include "ClearCacheCommand.h"
#include "storage/CacheManager.h"  // Concrete type for CRTP
#include "support/Utils.h"

ClearCacheCommand::ClearCacheCommand(CacheManager& cacheManager) : m_cacheManager(cacheManager) {}

void ClearCacheCommand::execute(const CommandContext& context) {
  if (strcmp_P(context.args, PSTR("yes")) != 0 &&
      strcmp_P(context.args, PSTR("confirm")) != 0) {
    Utils::ws_printf_P(context.client, PSTR("[WARNING] This will DELETE ALL CACHED DATA. To proceed, type: clearcache yes"));
    return;
  }
  m_cacheManager.reset();
  Utils::ws_printf_P(context.client, PSTR("Cache cleared successfully."));
}
