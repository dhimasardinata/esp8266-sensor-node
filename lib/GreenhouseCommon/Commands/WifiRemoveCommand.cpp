#include "WifiRemoveCommand.h"

#include "utils.h"

void WifiRemoveCommand::execute(const CommandContext& ctx) {
    if (!ctx.args || strlen(ctx.args) == 0) {
        Utils::ws_printf(ctx.client, "[ERROR] Usage: wifiremove <ssid>\n");
        Utils::ws_printf(ctx.client, "  Use 'wifilist' to see saved networks.\n");
        return;
    }
    
    // Handle quoted SSID
    char ssid[64];
    if (ctx.args[0] == '"') {
        const char* start = ctx.args + 1;
        const char* end = strchr(start, '"');
        if (end) {
            size_t len = static_cast<size_t>(end - start);
            if (len >= sizeof(ssid)) len = sizeof(ssid) - 1;
            strncpy(ssid, start, len);
            ssid[len] = '\0';
        } else {
            strncpy(ssid, start, sizeof(ssid) - 1);
        }
    } else {
        strncpy(ssid, ctx.args, sizeof(ssid) - 1);
    }
    ssid[sizeof(ssid) - 1] = '\0';
    
    // Check if it's a built-in network
    auto& store = m_wifiManager.getCredentialStore();
    const auto* primary = store.getPrimaryGH();
    const auto* secondary = store.getSecondaryGH();
    if ((primary && strcmp(ssid, primary->ssid) == 0) ||
        (secondary && strcmp(ssid, secondary->ssid) == 0)) {
        Utils::ws_printf(ctx.client, "[ERROR] Cannot remove built-in network '%s'.\n", ssid);
        Utils::ws_printf(ctx.client, "        Built-in networks (GH Atas/GH Bawah) are permanent.\n");
        return;
    }
    
    if (m_wifiManager.removeUserCredential(ssid)) {
        Utils::ws_printf(ctx.client, "[OK] Removed WiFi: %s\n", ssid);
    } else {
        Utils::ws_printf(ctx.client, "[ERROR] Network '%s' not found in saved list.\n", ssid);
        Utils::ws_printf(ctx.client, "        Use 'wifilist' to see saved networks.\n");
    }
}
