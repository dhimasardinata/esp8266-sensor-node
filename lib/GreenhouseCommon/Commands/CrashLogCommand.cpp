#include "CrashLogCommand.h"

#include "CrashHandler.h"
#include "utils.h"

void CrashLogCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend())
    return;

  class WsPrint : public Print {
  public:
    explicit WsPrint(AsyncWebSocketClient* c) : client(c) { buffer[0] = '\0'; }
    size_t write(uint8_t c) override {
      if (!client)
        return 0;
      if (idx >= sizeof(buffer) - 1) {
        flush();
      }
      buffer[idx++] = static_cast<char>(c);
      if (c == '\n') {
        flush();
      }
      return 1;
    }
    void flush() {
      if (!client || idx == 0)
        return;
      buffer[idx] = '\0';
      Utils::ws_send_encrypted(client, std::string_view(buffer, idx));
      idx = 0;
    }

  private:
    AsyncWebSocketClient* client = nullptr;
    char buffer[128];
    size_t idx = 0;
  };

  WsPrint out(context.client);
  (void)CrashHandler::streamLogTo(out);
  out.flush();
}

void ClearCrashCommand::execute(const CommandContext& context) {
  CrashHandler::clearLog();
  Utils::ws_printf(context.client, "Crash log deleted.");
}
