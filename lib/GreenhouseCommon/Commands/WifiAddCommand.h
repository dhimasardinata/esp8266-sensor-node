#ifndef WIFI_ADD_COMMAND_H
#define WIFI_ADD_COMMAND_H

#include "ICommand.h"
#include "WifiManager.h"

/**
 * @brief Add a new WiFi credential
 * Usage: wifiadd <ssid> <password>
 */
class WifiAddCommand : public ICommand {
public:
    explicit WifiAddCommand(WifiManager& wifiManager)
        : m_wifiManager(wifiManager) {}

    const char* getName() const override { return "wifiadd"; }
    const char* getDescription() const override { return "Add WiFi network: wifiadd <ssid> <password>"; }
    bool requiresAuth() const override { return true; }

    void execute(const CommandContext& ctx) override;

private:
    WifiManager& m_wifiManager;
};

#endif
