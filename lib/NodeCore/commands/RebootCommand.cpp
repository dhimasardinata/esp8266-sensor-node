#include "RebootCommand.h"

#include "REDACTED"
#include "support/Utils.h"

void RebootCommand::execute(const CommandContext& context) {
  Utils::ws_printf_P(context.client, PSTR("Rebooting now..."));
  BootGuard::setRebootReason(BootGuard::RebootReason::COMMAND);
  delay(100);
  ESP.restart();
}
