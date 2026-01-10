#include "WifiAddCommand.h"

#include "utils.h"

void WifiAddCommand::execute(const CommandContext& ctx) {
    // Parse SSID and password from args
    // Format: wifiadd "Network Name" password123
    // Or: wifiadd NetworkName password123
    
    if (!ctx.args || strlen(ctx.args) == 0) {
        Utils::ws_printf(ctx.client, "[ERROR] Usage: wifiadd <ssid> <password>\n");
        Utils::ws_printf(ctx.client, "  Example: wifiadd \"My Network\" mypassword\n");
        return;
    }
    
    char argsCopy[128];
    strncpy(argsCopy, ctx.args, sizeof(argsCopy) - 1);
    argsCopy[sizeof(argsCopy) - 1] = '\0';
    
    char* ssid = nullptr;
    char* password = nullptr;
    
    // Handle quoted SSID
    if (argsCopy[0] == '"') {
        ssid = argsCopy + 1;
        char* endQuote = strchr(ssid, '"');
        if (endQuote) {
            *endQuote = '\0';
            password = endQuote + 1;
            while (*password == ' ') password++;  // Skip spaces
        }
    } else {
        // Space-separated
        ssid = argsCopy;
        char* space = strchr(ssid, ' ');
        if (space) {
            *space = '\0';
            password = space + 1;
        }
    }
    
    if (!ssid || strlen(ssid) == 0) {
        Utils::ws_printf(ctx.client, "[ERROR] SSID cannot be empty.\n");
        return;
    }
    
    if (!password) {
        password = const_cast<char*>("");  // Allow open networks
    }
    
    // Validate lengths
    if (strlen(ssid) > 32) {
        Utils::ws_printf(ctx.client, "[ERROR] SSID too long (max 32 chars).\n");
        return;
    }
    
    if (strlen(password) > 64) {
        Utils::ws_printf(ctx.client, "[ERROR] Password too long (max 64 chars).\n");
        return;
    }
    
    // Add credential
    if (m_wifiManager.addUserCredential(ssid, password)) {
        Utils::ws_printf(ctx.client, "[OK] Added WiFi: %s\n", ssid);
        Utils::ws_printf(ctx.client, "     Will be tried on next connection attempt.\n");
    } else {
        Utils::ws_printf(ctx.client, "[ERROR] Failed to add. Storage may be full (max 5).\n");
        Utils::ws_printf(ctx.client, "     Use 'wifiremove <ssid>' to free a slot.\n");
    }
}
