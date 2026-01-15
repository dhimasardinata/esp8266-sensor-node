#include "WifiListCommand.h"

#include <ESP8266WiFi.h>

#include "TerminalFormatting.h"
#include "utils.h"

void WifiListCommand::execute(const CommandContext& ctx) {
  auto& store = m_wifiManager.getCredentialStore();

  TerminalFormat::printHeader(ctx.client, "WiFi Networks", "📡");
  TerminalFormat::printRow(ctx.client, "Current", 
    WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "(Not connected)");
  
  char rssi[16];
  snprintf(rssi, sizeof(rssi), "%d dBm", WiFi.RSSI());
  TerminalFormat::printRow(ctx.client, "RSSI", rssi);

  TerminalFormat::printSection(ctx.client, "Built-in");
  const auto* p = store.getPrimaryGH();
  const auto* s = store.getSecondaryGH();
  if (p) TerminalFormat::printListItem(ctx.client, 1, p->ssid, "[1st]", p->isAvailable);
  if (s) TerminalFormat::printListItem(ctx.client, 2, s->ssid, "[2nd]", s->isAvailable);

  TerminalFormat::printSection(ctx.client, "Saved");
  size_t n = 0;
  for (const auto& c : store.getSavedCredentials()) {
    if (!c.isEmpty()) TerminalFormat::printListItem(ctx.client, ++n + 2, c.ssid, nullptr, c.isAvailable);
  }
  if (n == 0) Utils::ws_printf(ctx.client, "  (None)\n");

  Utils::ws_printf(ctx.client, "\nwifiadd/wifiremove/openwifi\n");
}
