#include "REDACTED"

#include "utils.h"

namespace {
  void extractSsid(const char* args, char* ssid, size_t maxLen) {
    if (args[0] == '"') {
      const char* start = args + 1;
      const char* end = strchr(start, '"');
      size_t len = end ? static_cast<size_t>(end - start) : strnlen(start, maxLen - 1);
      if (len >= maxLen) len = maxLen - 1;
      memcpy(ssid, start, len);
      ssid[len] = REDACTED
    } else {
      size_t len = strnlen(args, maxLen - 1);
      memcpy(ssid, args, len);
      ssid[len] = REDACTED
    }
  }
}

void WifiRemoveCommand:REDACTED
  if (!ctx.args || strnlen(ctx.args, 1) == 0) {
    Utils::ws_printf(ctx.client, "[ERROR] Usage: wifiremove <ssid>\n");
    return;
  }

  char ssid[64];
  extractSsid(ctx.args, ssid, sizeof(ssid));

  auto& store = m_wifiManager.getCredentialStore();
  const auto* p = store.getPrimaryGH();
  const auto* s = store.getSecondaryGH();

  if ((p && strcmp(ssid, p->ssid) =REDACTED
    Utils::ws_printf(ctx.client, "[ERROR] Cannot remove built-in network '%s'.\n", ssid);
    return;
  }

  Utils::ws_printf(ctx.client, m_wifiManager.removeUserCredential(ssid)
    ? "[OK] Removed: %s\n" : "[ERROR] '%s' not found.\n", ssid);
}
