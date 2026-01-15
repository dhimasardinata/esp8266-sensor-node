#include "ClearCacheCommand.h"

#include "utils.h"

ClearCacheCommand::ClearCacheCommand(ICacheManager& cacheManager) : m_cacheManager(cacheManager) {}

void ClearCacheCommand::execute(const CommandContext& context) {
  if (strcmp(context.args, "yes") != 0 && strcmp(context.args, "confirm") != 0) {
    Utils::ws_printf(context.client, "[WARNING] This will DELETE ALL CACHED DATA. To proceed, type: clearcache yes");
    return;
  }
  m_cacheManager.reset();
  Utils::ws_printf(context.client, "Cache cleared successfully.");
}