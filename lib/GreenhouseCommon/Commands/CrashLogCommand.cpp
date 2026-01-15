#include "CrashLogCommand.h"

#include "CrashHandler.h"
#include "utils.h"

void CrashLogCommand::execute(const CommandContext& context) {
  String log = CrashHandler::getLog();
  if (log.length() > 1500) {
    Utils::ws_printf(context.client, "[WARN] Log too large (%u bytes), showing last 1KB:", log.length());
    log = log.substring(log.length() - 1000);
  }
  if (context.client && context.client->canSend()) {
    Utils::ws_printf(context.client, "%s", log.c_str());
  }
}

void ClearCrashCommand::execute(const CommandContext& context) {
  CrashHandler::clearLog();
  Utils::ws_printf(context.client, "Crash log deleted.");
}