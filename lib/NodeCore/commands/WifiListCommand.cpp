#include "REDACTED"

#include <ESP8266WiFi.h>
#include <cstring>

#include "terminal/TerminalFormatting.h"
#include "support/Utils.h"

namespace {
  size_t u32_to_dec(char* out, size_t out_len, uint32_t value) {
    if (!out || out_len == 0)
      return 0;
    char tmp[10];
    size_t n = 0;
    do {
      tmp[n++] = static_cast<char>('0' + (value % 10));
      value /= 10;
    } while (value != 0 && n < sizeof(tmp));
    size_t written = 0;
    while (n > 0 && written + 1 < out_len) {
      out[written++] = tmp[--n];
    }
    out[written] = '\0';
    return written;
  }

  void formatRssi(char* out, size_t out_len, int32_t rssi) {
    if (!out || out_len == 0)
      return;
    size_t pos = 0;
    if (rssi < 0) {
      if (pos + 1 >= out_len)
        return;
      out[pos++] = '-';
      rssi = -rssi;
    }
    pos += u32_to_dec(out + pos, (pos < out_len) ? (out_len - pos) : 0, static_cast<uint32_t>(rssi));
    if (pos + 4 >= out_len) {
      out[out_len - 1] = '\0';
      return;
    }
    out[pos++] = ' ';
    out[pos++] = 'd';
    out[pos++] = 'B';
    out[pos++] = 'm';
    out[pos] = '\0';
  }
}  // namespace

void WifiListCommand:REDACTED
  auto& store = m_wifiManager.getCredentialStore();
  const bool connected = (WiFi.status() == WL_CONNECTED);

  TerminalFormat::printHeader(ctx.client, "WiFi Networks", "📡");
  char ssid[WIFI_SSID_MAX_LEN] = REDACTED
  if (connected) {
    WiFi.SSID().toCharArray(ssid, sizeof(ssid));
    TerminalFormat::printRow(ctx.client, "Current", ssid);
  } else {
    TerminalFormat::printRow(ctx.client, "Current", F("(Not connected)"));
  }
  
  char rssi[16];
  if (connected) {
    formatRssi(rssi, sizeof(rssi), WiFi.RSSI());
    TerminalFormat::printRow(ctx.client, "RSSI", rssi);
  } else {
    TerminalFormat::printRow(ctx.client, "RSSI", F("-"));
  }

  auto isCurrentSsid = REDACTED
    return connected && !cred.isEmpty() && strncmp(cred.ssid, ssid, WIFI_SSID_MAX_LEN) =REDACTED
  };

  TerminalFormat::printSection(ctx.client, "Built-in");
  const auto* p = store.getPrimaryGH();
  const auto* s = store.getSecondaryGH();
  if (p) TerminalFormat::printListItem(ctx.client, 1, p->ssid, "[1st]", p->isAvailable() || isCurrentSsid(*p));
  if (s) TerminalFormat::printListItem(ctx.client, 2, s->ssid, "[2nd]", s->isAvailable() || isCurrentSsid(*s));

  TerminalFormat::printSection(ctx.client, "Saved");
  size_t n = 0;
  for (const auto& c : store.getSavedCredentials()) {
    if (!c.isEmpty()) {
      const bool available = c.isAvailable() || isCurrentSsid(c);
      TerminalFormat::printListItem(ctx.client, ++n + 2, c.ssid, nullptr, available);
    }
  }
  store.releaseSavedCredentials();
  if (n == 0) Utils::ws_printf_P(ctx.client, PSTR("  (None)\n"));

  Utils::ws_printf_P(ctx.client, PSTR("\nCommands: wifiadd | wifiremove | openwifi\n"));
}
