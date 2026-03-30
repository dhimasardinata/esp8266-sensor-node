#include "REDACTED"

#include "system/ConfigManager.h"
#include "support/Utils.h"

SetPortalPassCommand:REDACTED

void SetPortalPassCommand:REDACTED
  size_t passLen = REDACTED
  if (passLen < 8) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Portal password must be at least 8 characters."));
    return;
  }

  if (!Utils::isSafeString(std::string_view(context.args, passLen))) {
      Utils::ws_printf_P(context.client, PSTR("[ERROR] Password contains invalid characters."));
      return;
  }

  m_configManager.setPortalPassword(std:REDACTED

  if (m_configManager.save() == ConfigStatus::OK) {
    Utils::ws_printf_P(context.client, PSTR("Captive Portal password updated and saved."));
  } else {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Failed to save new portal password."));
  }

  m_configManager.releaseStrings();
}
