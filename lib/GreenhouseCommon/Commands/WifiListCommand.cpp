#include "WifiListCommand.h"

#include <ESP8266WiFi.h>

#include "TerminalFormatting.h"
#include "utils.h"

void WifiListCommand::execute(const CommandContext& ctx) {
    auto& store = m_wifiManager.getCredentialStore();
    
    // Use shared header formatting (DRY)
    TerminalFormat::printHeader(ctx.client, "WiFi Networks", "📡");
    
    // Current connection status
    TerminalFormat::printRow(ctx.client, "Current", 
        WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "(Not connected)");
    char rssiStr[16];
    snprintf(rssiStr, sizeof(rssiStr), "%d dBm", WiFi.RSSI());
    TerminalFormat::printRow(ctx.client, "RSSI", rssiStr);
    
    // Built-in credentials
    TerminalFormat::printSection(ctx.client, "Built-in (Priority)");
    const auto* primary = store.getPrimaryGH();
    const auto* secondary = store.getSecondaryGH();
    
    if (primary) TerminalFormat::printListItem(ctx.client, 1, primary->ssid, "[Primary]", primary->isAvailable);
    if (secondary) TerminalFormat::printListItem(ctx.client, 2, secondary->ssid, "[Secondary]", secondary->isAvailable);
    
    // User-saved credentials
    TerminalFormat::printSection(ctx.client, "Saved Networks");
    size_t savedCount = 0;
    for (const auto& cred : store.getSavedCredentials()) {
        if (!cred.isEmpty()) {
            savedCount++;
            TerminalFormat::printListItem(ctx.client, savedCount + 2, cred.ssid, nullptr, cred.isAvailable);
        }
    }
    
    if (savedCount == 0) {
        Utils::ws_printf(ctx.client, "  (None saved)\n");
    }
    
    // Commands help
    TerminalFormat::printSection(ctx.client, "Commands");
    Utils::ws_printf(ctx.client, "  wifiadd <ssid> <password> - Add network\n");
    Utils::ws_printf(ctx.client, "  wifiremove <ssid>         - Remove saved\n");
    Utils::ws_printf(ctx.client, "  openwifi                  - Open portal\n");
    Utils::ws_printf(ctx.client, "\n");
}

