#include "SetTimeCommand.h"

#include <time.h>

#include "net/NtpClient.h"
#include "support/Utils.h"

namespace {
  bool is_digits(const char* s) {
    if (!s || *s == '\0')
      return false;
    for (const char* p = s; *p; ++p) {
      if (*p < '0' || *p > '9')
        return false;
    }
    return true;
  }
}

void SetTimeCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend())
    return;

  const char* args = context.args ? context.args : "";
  while (*args == ' ')
    ++args;

  if (*args == '\0') {
    Utils::ws_printf_P(context.client,
                       PSTR("[ERROR] Usage: settime <epoch> OR settime YYYY-MM-DD HH:MM:SS"));
    return;
  }

  // If first token is all digits, treat as epoch seconds.
  char arg0[20] = {0};
  char arg1[20] = {0};
  if (sscanf(args, "%19s %19s", arg0, arg1) <= 0) {
    Utils::ws_printf_P(context.client,
                       PSTR("[ERROR] Usage: settime <epoch> OR settime YYYY-MM-DD HH:MM:SS"));
    return;
  }

  time_t epoch = 0;
  if (is_digits(arg0) && (arg1[0] == '\0')) {
    char* endptr = nullptr;
    unsigned long v = strtoul(arg0, &endptr, 10);
    if (endptr && *endptr == '\0') {
      epoch = static_cast<time_t>(v);
    }
  } else {
    int y = 0, m = 0, d = 0, hh = 0, mm = 0, ss = 0;
    int parsed = sscanf(args, "%d-%d-%d %d:%d:%d", &y, &m, &d, &hh, &mm, &ss);
    if (parsed != 6) {
      parsed = sscanf(args, "%d-%d-%dT%d:%d:%d", &y, &m, &d, &hh, &mm, &ss);
    }
    if (parsed == 6) {
      tm t{};
      t.tm_year = y - 1900;
      t.tm_mon = m - 1;
      t.tm_mday = d;
      t.tm_hour = hh;
      t.tm_min = mm;
      t.tm_sec = ss;
      t.tm_isdst = -1;
      epoch = mktime(&t);
    }
  }

  if (epoch <= 0) {
    Utils::ws_printf_P(context.client,
                       PSTR("[ERROR] Invalid time. Use epoch or 'YYYY-MM-DD HH:MM:SS'."));
    return;
  }

  m_ntpClient.setManualTime(epoch);
  Utils::ws_printf_P(context.client, PSTR("[SUCCESS] Time set. Epoch=%lu"), (unsigned long)epoch);
}
