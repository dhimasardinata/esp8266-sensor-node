#include "SendNowCommand.h"

#include "ApiClient.h"
#include "utils.h"

SendNowCommand::SendNowCommand(ApiClient& apiClient) : m_apiClient(apiClient) {}

void SendNowCommand::execute(const CommandContext& context) {
  Utils::ws_printf(context.client,
                   "Scheduling an immediate data upload attempt...\n"
                   "Task scheduled. The result will appear in the next background sync cycle. Check serial "
                   "monitor for details.");

  m_apiClient.scheduleImmediateUpload();
}