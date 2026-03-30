#include "SendNowCommand.h"

#include "api/ApiClient.h"
#include "config/constants.h"
#include "support/Utils.h"
#include "system/Logger.h"

SendNowCommand::SendNowCommand(ApiClient& apiClient) : m_apiClient(apiClient) {}

void SendNowCommand::execute(const CommandContext& context) {
  LOG_INFO("CMD", F("sendnow command executed"));
  Utils::ws_printf_P(context.client, PSTR("Sending data now...\n"));

  // If heap is low, close the terminal session to free buffers before TLS upload.
  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t maxBlock = ESP.getMaxFreeBlockSize();
  uint32_t minBlock = AppConstants::TLS_MIN_SAFE_BLOCK_SIZE;
  uint32_t minTotal = REDACTED
  if (context.client) {
    // Account for active WS session overhead (single client).
    minBlock += 512;
    minTotal += REDACTED
  }
  bool restoreWsAfterUpload = false;
  if (maxBlock < minBlock || freeHeap < minTotal) {
    LOG_WARN("CMD",
             F("sendnow low-heap guard hit (free=%u, block=%u, need=%u/%u)"),
             freeHeap,
             maxBlock,
             minTotal,
             minBlock);
    Utils::ws_printf_P(context.client,
                       PSTR("[WARN] Low heap (free=%u, block=%u). Closing terminal to free RAM..."),
                       freeHeap,
                       maxBlock);
    if (context.client) {
      context.client->close();
    }
    (void)Utils::ws_set_enabled(false);
    restoreWsAfterUpload = true;
  }

  m_apiClient.requestImmediateUpload(restoreWsAfterUpload);
}
