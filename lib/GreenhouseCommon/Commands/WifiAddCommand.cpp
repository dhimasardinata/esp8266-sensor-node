#include "WifiAddCommand.h"

#include "utils.h"

namespace {
  bool parseWifiArgs(char* buf, char*& ssid, char*& pass) {
    ssid = nullptr;
    pass = nullptr;
    
    if (buf[0] == '"') {
      ssid = buf + 1;
      char* eq = strchr(ssid, '"');
      if (eq) { *eq = '\0'; pass = eq + 1; while (*pass == ' ') pass++; }
    } else {
      ssid = buf;
      char* sp = strchr(ssid, ' ');
      if (sp) { *sp = '\0'; pass = sp + 1; }
    }
    return ssid && strnlen(ssid, 33) > 0;
  }
  
  bool validateCreds(const char* ssid, const char* pass, AsyncWebSocketClient* c) {
    if (strnlen(ssid, 33) > 32) { Utils::ws_printf(c, "[ERROR] SSID too long (max 32).\n"); return false; }
    if (strnlen(pass, 65) > 64) { Utils::ws_printf(c, "[ERROR] Password too long (max 64).\n"); return false; }
    return true;
  }
}

void WifiAddCommand::execute(const CommandContext& ctx) {
  if (!ctx.args || strnlen(ctx.args, 1) == 0) {
    Utils::ws_printf(ctx.client, "[ERROR] Usage: wifiadd <ssid> <password> [-h]\n");
    return;
  }

  char buf[128];
  snprintf(buf, sizeof(buf), "%s", ctx.args);

  char* ssid = nullptr;
  char* password = nullptr;
  bool hidden = false;

  // Check for -h flag
  char* hFlag = strstr(buf, " -h");
  if (hFlag) {
      *hFlag = '\0'; // Remove flag from string
      hidden = true;
  }

  if (!parseWifiArgs(buf, ssid, password)) {
    Utils::ws_printf(ctx.client, "[ERROR] SSID cannot be empty.\n");
    return;
  }

  if (!password) password = const_cast<char*>("");
  if (!validateCreds(ssid, password, ctx.client)) return;

  if (m_wifiManager.addUserCredential(ssid, password, hidden)) {
    Utils::ws_printf(ctx.client, "[OK] Added WiFi: %s%s\n", ssid, hidden ? " (hidden)" : "");
  } else {
    Utils::ws_printf(ctx.client, "[ERROR] Storage full (max 5). Use wifiremove.\n");
  }
}
