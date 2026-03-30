#ifndef FORCE_OTA_INSECURE_COMMAND_H
#define FORCE_OTA_INSECURE_COMMAND_H

#include "ICommand.h"
#include "REDACTED"
#include "support/Utils.h"

class ForceOtaInsecureCommand final : REDACTED
public:
  explicit ForceOtaInsecureCommand(OtaManager& otaManager) : REDACTED

  // REQUIRED BY ICOMMAND INTERFACE
    PGM_P getName_P() const override { return PSTR("REDACTED"); }
    PGM_P getDescription_P() const override {
    return PSTR("REDACTED");
  }
  CommandSection helpSection() const override { return CommandSection::SYSTEM; }
    bool requiresAuth() const override {
    return true;
  }
  uint32_t getNameHash() const override {
    return CompileTimeUtils::ct_hash("force-ota-insecure");
  }

  // FIXED SIGNATURE: Must be 'const CommandContext&'
  void execute(const CommandContext& ctx) override {
    Utils::ws_printf_P(ctx.client, PSTR("[WARN] Initializing Insecure OTA... SSL Validation Bypassed."));
    // Logic to trigger the update
    m_otaManager.forceUpdateCheck();
  }

private:
  OtaManager& m_otaManager;
};

#endif
