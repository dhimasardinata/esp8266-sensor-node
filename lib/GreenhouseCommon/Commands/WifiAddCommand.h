#ifndef WIFI_ADD_COMMAND_H
#define WIFI_ADD_COMMAND_H

#include "ICommand.h"
#include "REDACTED"

// Add a new WiFi credential.
// Usage: wifiadd <ssid> <password>
class WifiAddCommand : REDACTED
public:
    explicit WifiAddCommand(WifiManager& wifiManager)
        : m_wifiManager(wifiManager) {}

    const char* getName() const override { return "REDACTED"; }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("wifiadd"); }
    const char* getDescription() const override { return "Add WiFi network: REDACTED
    bool requiresAuth() const override { return true; }

    void execute(const CommandContext& ctx) override;

private:
    WifiManager& m_wifiManager;
};

#endif
