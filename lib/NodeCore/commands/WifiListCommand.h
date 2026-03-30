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

      PGM_P getName_P() const override { return PSTR("REDACTED"); }
      uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("wifilist"); }
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

#endif
