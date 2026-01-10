#include "OpenWifiCommand.h"

#include <ESP8266WiFi.h>

#include "utils.h"

void OpenWifiCommand::execute(const CommandContext& ctx) {
    Utils::ws_printf(ctx.client, 
        "[WIFI] Current state: %s\n"
        "[WIFI] SSID: %s | RSSI: %d dBm\n",
        WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected",
        WiFi.SSID().c_str(),
        WiFi.RSSI()
    );
    
    // Force disconnect and open portal
    Utils::ws_printf(ctx.client, "[WIFI] Forcing portal open...\n");
    
    WiFi.disconnect(false);  // Disconnect but don't clear credentials
    m_wifiManager.startPortal();
    
    Utils::ws_printf(ctx.client, 
        "[WIFI] ✓ Portal opened!\n"
        "[WIFI] Connect to AP: %s\n"
        "[WIFI] Go to: http://192.168.1.100\n",
        WiFi.softAPSSID().c_str()
    );
}
