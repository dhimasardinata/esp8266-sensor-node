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

      PGM_P getName_P() const override { return PSTR("REDACTED"); }
      uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("wifiadd"); }
      PGM_P getDescription_P() const override {
    return PSTR("Add WiFi network: REDACTED
  }
  CommandSection helpSection() const override { return CommandSection::WIFI; }
      bool requiresAuth() const override {
    return true;
  }

    void execute(const CommandContext& ctx) override;

private:
    WifiManager& m_wifiManager;
};

#endif
