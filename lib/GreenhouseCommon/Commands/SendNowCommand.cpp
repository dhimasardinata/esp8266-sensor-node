#include "SendNowCommand.h"

#include "ApiClient.h"
#include "constants.h"
#include "utils.h"
#include "Logger.h"

SendNowCommand::SendNowCommand(ApiClient& apiClient) : m_apiClient(apiClient) {}

void SendNowCommand::execute(const CommandContext& context) {
  LOG_INFO("CMD", F("sendnow command executed"));
  Utils::ws_printf(context.client, "Sending data now...\n");

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
  if (maxBlock < minBlock || freeHeap < minTotal) {
    Utils::ws_printf(context.client,
                     "[WARN] Low heap (free=%u, block=%u). Closing terminal to free RAM...",
                     freeHeap,
                     maxBlock);
    if (context.client) {
      context.client->close();
    }
    (void)Utils::ws_set_enabled(false);
  }

  m_apiClient.requestImmediateUpload();
}
