#include "REDACTED"

#include <ESP8266WiFi.h>

#include "utils.h"

void OpenWifiCommand:REDACTED
    char ssid[WIFI_SSID_MAX_LEN] = REDACTED
    if (WiFi.status() =REDACTED
        WiFi.SSID().toCharArray(ssid, sizeof(ssid));
    }

    Utils::ws_printf(ctx.client, 
        "[WIFI] Current state: REDACTED
        "[WIFI] SSID: REDACTED
        WiFi.status() =REDACTED
        ssid,
        WiFi.RSSI()
    );
    
    // Force disconnect and open portal
    Utils::ws_printf(ctx.client, "[WIFI] Forcing portal open...\n");
    
    WiFi.disconnect(false);  // Disconnect but don't clear credentials
    m_wifiManager.startPortal();
    
    char apSsid[WIFI_SSID_MAX_LEN] = REDACTED
    WiFi.softAPSSID().toCharArray(apSsid, sizeof(apSsid));
    Utils::ws_printf(ctx.client, 
        "REDACTED"
        "[WIFI] Connect to AP: REDACTED
        "[WIFI] Go to: REDACTED
        apSsid
    );
}
