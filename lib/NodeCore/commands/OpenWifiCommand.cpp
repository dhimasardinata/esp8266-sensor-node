#include "REDACTED"

#include <ESP8266WiFi.h>

#include "support/Utils.h"

void OpenWifiCommand:REDACTED
    char ssid[WIFI_SSID_MAX_LEN] = REDACTED
    char wifiState[16];
    if (WiFi.status() =REDACTED
        WiFi.SSID().toCharArray(ssid, sizeof(ssid));
    }
    strncpy_P(wifiState,
              WiFi.status() =REDACTED
              sizeof(wifiState) - 1);
    wifiState[sizeof(wifiState) - 1] = REDACTED

    Utils::ws_printf_P(ctx.client,
        PSTR("[WIFI] Current state: REDACTED
             "[WIFI] SSID: REDACTED
        wifiState,
        ssid,
        WiFi.RSSI()
    );
    
    // Force disconnect and open portal
    Utils::ws_printf_P(ctx.client, PSTR("[WIFI] Forcing portal open...\n"));
    
    WiFi.disconnect(false);  // Disconnect but don't clear credentials
    m_wifiManager.startPortal();
    
    char apSsid[WIFI_SSID_MAX_LEN] = REDACTED
    WiFi.softAPSSID().toCharArray(apSsid, sizeof(apSsid));
    Utils::ws_printf_P(ctx.client,
        PSTR("REDACTED"
             "[WIFI] Connect to AP: REDACTED
             "[WIFI] Go to: REDACTED
        apSsid
    );
}
