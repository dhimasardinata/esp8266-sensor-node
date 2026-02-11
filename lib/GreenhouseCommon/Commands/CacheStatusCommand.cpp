#include "CacheStatusCommand.h"
#include "CacheManager.h"  // Concrete type for CRTP
#include "utils.h"

CacheStatusCommand::CacheStatusCommand(CacheManager& cacheManager) : m_cacheManager(cacheManager) {}

void CacheStatusCommand::execute(const CommandContext& context) {
  uint32_t size_bytes, head, tail;
  m_cacheManager.get_status(size_bytes, head, tail);

  if (!context.client || !context.client->canSend()) {
    return;
  }

  Utils::ws_printf(
      context.client, "Cache Status:\n  Size: %u bytes\n  Head: %u\n  Tail: %u", size_bytes, head, tail);
}