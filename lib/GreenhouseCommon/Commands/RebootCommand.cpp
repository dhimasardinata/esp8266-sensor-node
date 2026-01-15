#include "RebootCommand.h"

#include "utils.h"

void RebootCommand::execute(const CommandContext& context) {
  Utils::ws_printf(context.client, "Rebooting now...");
  delay(100);
  ESP.restart();
}