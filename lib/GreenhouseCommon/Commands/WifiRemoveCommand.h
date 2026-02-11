#ifndef WIFI_REMOVE_COMMAND_H
#define WIFI_REMOVE_COMMAND_H

#include "ICommand.h"
#include "REDACTED"

// Remove a saved WiFi credential.
// Usage: wifiremove <ssid>
class WifiRemoveCommand : REDACTED
public:
    explicit WifiRemoveCommand(WifiManager& wifiManager)
        : m_wifiManager(wifiManager) {}

    const char* getName() const override { return "REDACTED"; }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("wifiremove"); }
    const char* getDescription() const override { return "Remove saved WiFi: REDACTED
    bool requiresAuth() const override { return true; }

    void execute(const CommandContext& ctx) override;

private:
    WifiManager& m_wifiManager;
};

#endif
