#ifndef OPEN_WIFI_COMMAND_H
#define OPEN_WIFI_COMMAND_H

#include "ICommand.h"
#include "WifiManager.h"

/**
 * @brief Command to force open WiFi portal even when connected
 * 
 * Allows user to reconfigure WiFi settings at any time.
 * Usage: openwifi
 */
class OpenWifiCommand : public ICommand {
public:
    explicit OpenWifiCommand(WifiManager& wifiManager)
        : m_wifiManager(wifiManager) {}

    const char* getName() const override { return "openwifi"; }
    const char* getDescription() const override { 
        return "Force open WiFi portal (even if connected)"; 
    }
    bool requiresAuth() const override { return true; }

    void execute(const CommandContext& ctx) override;

private:
    WifiManager& m_wifiManager;
};

#endif // OPEN_WIFI_COMMAND_H
