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

      PGM_P getName_P() const override { return PSTR("REDACTED"); }
      uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("wifiremove"); }
      PGM_P getDescription_P() const override {
    return PSTR("Remove saved WiFi: REDACTED
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
