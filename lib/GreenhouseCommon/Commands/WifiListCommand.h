#ifndef WIFI_LIST_COMMAND_H
#define WIFI_LIST_COMMAND_H

#include "ICommand.h"
#include "REDACTED"

// Lists all saved WiFi credentials and their status.
// Usage: wifilist
class WifiListCommand : REDACTED
public:
    explicit WifiListCommand(WifiManager& wifiManager)
        : m_wifiManager(wifiManager) {}

    const char* getName() const override { return "REDACTED"; }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("wifilist"); }
    const char* getDescription() const override { return "REDACTED"; }
    bool requiresAuth() const override { return true; }

    void execute(const CommandContext& ctx) override;

private:
    WifiManager& m_wifiManager;
};

#endif
