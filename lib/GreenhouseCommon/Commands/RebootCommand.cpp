#include "RebootCommand.h"

#include "BootGuard.h"
#include "utils.h"

void RebootCommand::execute(const CommandContext& context) {
  Utils::ws_printf(context.client, "Rebooting now...");
  BootGuard::setRebootReason(BootGuard::RebootReason::COMMAND);
  delay(100);
  ESP.restart();
}