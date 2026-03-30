#ifndef OPEN_WIFI_COMMAND_H
#define OPEN_WIFI_COMMAND_H

#include "ICommand.h"
#include "REDACTED"

// Command to force open WiFi portal even when connected.
// Allows user to reconfigure WiFi settings at any time.
// Usage: openwifi
class OpenWifiCommand : REDACTED
public:
    explicit OpenWifiCommand(WifiManager& wifiManager)
        : m_wifiManager(wifiManager) {}

      PGM_P getName_P() const override { return PSTR("REDACTED"); }
      uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("openwifi"); }
      PGM_P getDescription_P() const override {
    return PSTR("REDACTED");
  }
  CommandSection helpSection() const override { return CommandSection::WIFI; }
      bool requiresAuth() const override {
    return true;
  }

    void execute(const CommandContext& ctx) override;

private:
    WifiManager& m_wifiManager;
};

#endif // OPEN_WIFI_COMMAND_H
