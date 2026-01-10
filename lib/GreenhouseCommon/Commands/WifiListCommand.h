#ifndef WIFI_LIST_COMMAND_H
#define WIFI_LIST_COMMAND_H

#include "ICommand.h"
#include "WifiManager.h"

/**
 * @brief Lists all saved WiFi credentials and their status
 * Usage: wifilist
 */
class WifiListCommand : public ICommand {
public:
    explicit WifiListCommand(WifiManager& wifiManager)
        : m_wifiManager(wifiManager) {}

    const char* getName() const override { return "wifilist"; }
    const char* getDescription() const override { return "List saved WiFi networks"; }
    bool requiresAuth() const override { return true; }

    void execute(const CommandContext& ctx) override;

private:
    WifiManager& m_wifiManager;
};

#endif
