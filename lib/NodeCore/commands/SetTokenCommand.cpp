#include "REDACTED"

#include <cstring>
#include <strings.h>

#include "system/ConfigManager.h"
#include "support/Utils.h"

namespace {
  enum class Scope : uint8_t { Upload, Ota, Both };
  enum class Action : uint8_t { SetValue, Show, UseDefault, Clear };

#ifndef FACTORY_API_TOKEN
#define FACTORY_API_TOKEN "REDACTED"
#endif

#ifndef FACTORY_OTA_TOKEN
#define FACTORY_OTA_TOKEN "REDACTED"
#endif

  const char* skipSpaces(const char* s) {
    while (s && *s == ' ') {
      ++s;
    }
    return s ? s : "";
  }

  const char* nextToken(const char* s, char* out, size_t out_len) {
    if (!out || out_len == 0) {
      return s;
    }
    out[0] = '\0';
    s = skipSpaces(s);
    size_t pos = 0;
    while (*s != '\0' && *s != ' ') {
      if (pos + 1 < out_len) {
        out[pos++] = *s;
      }
      ++s;
    }
    out[pos] = '\0';
    return skipSpaces(s);
  }

  bool looksLikeToken(const char* token) {
    return token && strchr(token, '|') != REDACTED
  }

  const char* getUploadTokenState(ConfigManager& configManager) {
    const char* uploadToken = REDACTED
    const bool uploadEmpty = (!uploadToken || uploadToken[0] == '\0');
    const bool uploadFactory = (!uploadEmpty && strcmp(uploadToken, FACTORY_API_TOKEN) == 0);
    return uploadEmpty ? "empty" : (uploadFactory ? "factory-default" : "set");
  }

  const char* getOtaTokenState(ConfigManager& configManager) {
    return configManager.hasOtaTokenOverride() ? "override-set" : REDACTED
  }

  const char* getEffectiveUploadSource(ConfigManager& configManager) {
    const char* uploadToken = REDACTED
    if (uploadToken && uploadToken[0] != REDACTED
      return "upload-slot";
    }
    if (configManager.hasOtaTokenOverride()) {
      return "REDACTED";
    }
    if (FACTORY_OTA_TOKEN[0] != REDACTED
      return "REDACTED";
    }
    return "none";
  }

  const char* getEffectiveOtaSource(ConfigManager& configManager) {
    if (configManager.hasOtaTokenOverride()) {
      return "REDACTED";
    }
    if (FACTORY_OTA_TOKEN[0] != REDACTED
      return "REDACTED";
    }
    const char* uploadToken = REDACTED
    if (uploadToken && uploadToken[0] != REDACTED
      return "upload-slot";
    }
    return "none";
  }

  void printTokenSummary(AsyncWebSocketClient* client, Scope scope, ConfigManager& configManager) {
    const char* uploadState = getUploadTokenState(configManager);
    const char* otaState = REDACTED
    const char* uploadSource = getEffectiveUploadSource(configManager);
    const char* otaSource = REDACTED

    switch (scope) {
      case Scope::Upload:
        Utils::ws_printf_P(client, PSTR("Configured Upload Token: %s\n"), uploadState);
        Utils::ws_printf_P(client, PSTR("Effective Upload Token Source: %s\n"), uploadSource);
        break;
      case Scope::Ota:
        Utils::ws_printf_P(client, PSTR("Configured OTA Token: %s\n"), otaState);
        Utils::ws_printf_P(client, PSTR("Effective OTA Token Source: %s\n"), otaSource);
        break;
      case Scope::Both:
        Utils::ws_printf_P(client, PSTR("Configured Upload Token: %s\n"), uploadState);
        Utils::ws_printf_P(client, PSTR("Configured OTA Token: %s\n"), otaState);
        Utils::ws_printf_P(client, PSTR("Effective Upload Token Source: %s\n"), uploadSource);
        Utils::ws_printf_P(client, PSTR("Effective OTA Token Source: %s\n"), otaSource);
        break;
    }

    configManager.releaseStrings();
  }
}  // namespace

SetTokenCommand:REDACTED

void SetTokenCommand:REDACTED
  if (!context.client || !context.client->canSend()) {
    return;
  }

  const char* args = context.args ? skipSpaces(context.args) : "";
  if (*args == '\0') {
    Utils::ws_printf_P(context.client,
                       PSTR("[ERROR] Usage: settoken show | settoken [upload|ota|both] <token|default|none|show>\n"));
    return;
  }

  Scope scope = Scope::Upload;  // backward-compatible default
  Action action = Action::SetValue;

  char first[8] = {0};
  const char* value = nextToken(args, first, sizeof(first));

  if (strcasecmp(first, "show") == 0) {
    action = Action::Show;
    scope = Scope::Both;
    value = "";
  } else if (strcasecmp(first, "upload") == 0) {
    scope = Scope::Upload;
  } else if (strcasecmp(first, "ota") =REDACTED
    scope = Scope::Ota;
  } else if (strcasecmp(first, "both") == 0) {
    scope = Scope::Both;
  } else {
    value = args;
  }

  value = skipSpaces(value);
  if (action != Action::Show && *value == '\0') {
    Utils::ws_printf_P(context.client,
                       PSTR("[ERROR] Usage: settoken show | settoken [upload|ota|both] <token|default|none|show>\n"));
    return;
  }

  if (action != Action::Show) {
    if (strcasecmp(value, "show") == 0) {
      action = Action::Show;
    } else if (strcasecmp(value, "default") == 0 || strcasecmp(value, "defaults") == 0) {
      action = Action::UseDefault;
    } else if (strcasecmp(value, "none") == 0 || strcasecmp(value, "clear") == 0) {
      action = Action::Clear;
    } else if (scope == Scope::Ota &&
               (strcasecmp(value, "inherit") == 0 || strcasecmp(value, "shared") == 0)) {
      action = Action::UseDefault;
    }
  }

  if (action == Action::Show) {
    printTokenSummary(context.client, scope, m_configManager);
    return;
  }

  if (action == Action::SetValue && !looksLikeToken(value)) {
    Utils::ws_printf_P(context.client,
                       PSTR("REDACTED"));
  }

  if (action == Action::UseDefault) {
    switch (scope) {
      case Scope::Upload:
        m_configManager.setAuthToken(FACTORY_API_TOKEN);
        break;
      case Scope::Ota:
        m_configManager.clearOtaAuthToken();
        break;
      case Scope::Both:
        m_configManager.setAuthToken(FACTORY_API_TOKEN);
        m_configManager.clearOtaAuthToken();
        break;
    }
  } else if (action == Action::Clear) {
    switch (scope) {
      case Scope::Upload:
        m_configManager.setAuthToken("REDACTED");
        break;
      case Scope::Ota:
        m_configManager.clearOtaAuthToken();
        break;
      case Scope::Both:
        m_configManager.setAuthToken("REDACTED");
        m_configManager.clearOtaAuthToken();
        break;
    }
  } else {
    switch (scope) {
      case Scope::Upload:
        m_configManager.setAuthToken(value);
        break;
      case Scope::Ota:
        m_configManager.setOtaAuthToken(value);
        break;
      case Scope::Both:
        m_configManager.setAuthToken(value);
        m_configManager.setOtaAuthToken(value);
        break;
    }
  }

  if (m_configManager.save() == ConfigStatus::OK) {
    switch (scope) {
      case Scope::Upload:
        Utils::ws_printf_P(context.client,
                           action == Action::UseDefault ? PSTR("Upload token restored to factory default.\n")
                           : action == Action::Clear ? PSTR("Upload token cleared.\n")
                                                    : PSTR("Upload token updated and saved.\n"));
        break;
      case Scope::Ota:
        Utils::ws_printf_P(context.client,
                           action == Action::UseDefault ? PSTR("OTA token override cleared. OTA now follows factory/upload fallback.\n")
                           : action == Action::Clear ? PSTR("OTA token override cleared. OTA now follows factory/upload fallback.\n")
                                                    : PSTR("OTA token updated and saved.\n"));
        break;
      case Scope::Both:
        Utils::ws_printf_P(context.client,
                           action == Action::UseDefault ? PSTR("Upload and OTA tokens restored to defaults.\n")
                           : action == Action::Clear ? PSTR("Upload token cleared and OTA override cleared.\n")
                                                    : PSTR("Upload and OTA tokens updated and saved.\n"));
        break;
    }
  } else {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Failed to save token.\n"));
  }

  m_configManager.releaseStrings();
}
