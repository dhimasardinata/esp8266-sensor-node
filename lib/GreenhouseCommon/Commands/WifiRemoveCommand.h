#ifndef WIFI_REMOVE_COMMAND_H
#define WIFI_REMOVE_COMMAND_H

#include "ICommand.h"
#include "WifiManager.h"

/**
 * @brief Remove a saved WiFi credential
 * Usage: wifiremove <ssid>
 */
class WifiRemoveCommand : public ICommand {
public:
    explicit WifiRemoveCommand(WifiManager& wifiManager)
        : m_wifiManager(wifiManager) {}

    const char* getName() const override { return "wifiremove"; }
    const char* getDescription() const override { return "Remove saved WiFi: wifiremove <ssid>"; }
    bool requiresAuth() const override { return true; }

    void execute(const CommandContext& ctx) override;

private:
    WifiManager& m_wifiManager;
};

#endif
