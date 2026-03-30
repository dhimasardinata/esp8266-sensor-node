#include "SetUrlCommand.h"

#include <cstring>
#include <strings.h>

#include "system/ConfigManager.h"
#include "support/Utils.h"

namespace {
  enum class Scope : uint8_t { Upload, Ota, Both };
  enum class Action : uint8_t { SetValue, Show, UseDefault, Clear };

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

  bool startsWithHttp(const char* url) {
    return url &&
           (strncasecmp(url, "https://", 8) == 0 || strncasecmp(url, "http://", 7) == 0);
  }

  void printScopeUrls(AsyncWebSocketClient* client,
                      Scope scope,
                      const char* uploadUrl,
                      const char* otaUrl) {
    const char* displayUpload = (uploadUrl && uploadUrl[0] != '\0') ? uploadUrl : "<none>";
    const char* displayOta = REDACTED
    switch (scope) {
      case Scope::Upload:
        Utils::ws_printf_P(client, PSTR("Configured Upload URL: %s\n"), displayUpload);
        break;
      case Scope::Ota:
        Utils::ws_printf_P(client, PSTR("Configured OTA URL: %s\n"), displayOta);
        break;
      case Scope::Both:
        Utils::ws_printf_P(client, PSTR("Configured Upload URL: %s\n"), displayUpload);
        Utils::ws_printf_P(client, PSTR("Configured OTA URL: %s\n"), displayOta);
        break;
    }
  }
}  // namespace

SetUrlCommand::SetUrlCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void SetUrlCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) {
    return;
  }

  const char* args = context.args ? skipSpaces(context.args) : "";
  if (*args == '\0') {
    Utils::ws_printf_P(
        context.client,
        PSTR("[ERROR] Usage: seturl show | seturl [upload|ota|both] <url|default|none|show>\n"));
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
    Utils::ws_printf_P(
        context.client,
        PSTR("[ERROR] Usage: seturl show | seturl [upload|ota|both] <url|default|none|show>\n"));
    return;
  }

  if (action != Action::Show) {
    if (strcasecmp(value, "show") == 0) {
      action = Action::Show;
    } else if (strcasecmp(value, "default") == 0 || strcasecmp(value, "defaults") == 0) {
      action = Action::UseDefault;
    } else if (strcasecmp(value, "none") == 0 || strcasecmp(value, "clear") == 0) {
      action = Action::Clear;
    }
  }

  if (action == Action::Show) {
    const char* uploadUrl = m_configManager.getDataUploadUrl();
    const char* otaUrl = REDACTED
    printScopeUrls(context.client, scope, uploadUrl, otaUrl);
    m_configManager.releaseStrings();
    return;
  }

  if (action == Action::SetValue && !startsWithHttp(value)) {
    Utils::ws_printf_P(context.client,
                       PSTR("[ERROR] URL must start with http:// or https://, or use 'default'/'none'.\n"));
    return;
  }

  if (action == Action::SetValue && strnlen(value, MAX_URL_LEN + 1) > MAX_URL_LEN - 1) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] URL too long (max %u chars).\n"), (unsigned)(MAX_URL_LEN - 1));
    return;
  }

  if (action == Action::SetValue && !m_configManager.getConfig().ALLOW_INSECURE_HTTPS() &&
      strncasecmp(value, "http://", 7) == 0) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Plain HTTP URL blocked while insecure HTTPS is disabled.\n"));
    return;
  }

  if (action == Action::UseDefault) {
    switch (scope) {
      case Scope::Upload:
        m_configManager.setDataUploadUrl(DEFAULT_DATA_URL);
        break;
      case Scope::Ota:
        m_configManager.setOtaUrlBase(DEFAULT_OTA_URL_BASE);
        break;
      case Scope::Both:
        m_configManager.setDataUploadUrl(DEFAULT_DATA_URL);
        m_configManager.setOtaUrlBase(DEFAULT_OTA_URL_BASE);
        break;
    }
  } else if (action == Action::Clear) {
    switch (scope) {
      case Scope::Upload:
        m_configManager.setDataUploadUrl("");
        break;
      case Scope::Ota:
        m_configManager.setOtaUrlBase("REDACTED");
        break;
      case Scope::Both:
        m_configManager.setDataUploadUrl("");
        m_configManager.setOtaUrlBase("REDACTED");
        break;
    }
  } else {
    switch (scope) {
      case Scope::Upload:
        m_configManager.setDataUploadUrl(value);
        break;
      case Scope::Ota:
        m_configManager.setOtaUrlBase(value);
        break;
      case Scope::Both:
        m_configManager.setDataUploadUrl(value);
        m_configManager.setOtaUrlBase(value);
        break;
    }
  }

  const ConfigStatus status = m_configManager.save();
  m_configManager.releaseStrings();

  if (status != ConfigStatus::OK) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Failed to save URL change.\n"));
    return;
  }

  switch (scope) {
    case Scope::Upload:
      Utils::ws_printf_P(context.client,
                         action == Action::UseDefault ? PSTR("Upload URL restored to default.\n")
                         : action == Action::Clear ? PSTR("Upload URL override cleared.\n")
                                                  : PSTR("Upload URL updated and saved.\n"));
      break;
    case Scope::Ota:
      Utils::ws_printf_P(context.client,
                         action == Action::UseDefault ? PSTR("OTA URL restored to default.\n")
                         : action == Action::Clear ? PSTR("OTA URL override cleared.\n")
                                                  : PSTR("OTA URL updated and saved.\n"));
      break;
    case Scope::Both:
      Utils::ws_printf_P(context.client,
                         action == Action::UseDefault ? PSTR("Upload and OTA URLs restored to defaults.\n")
                         : action == Action::Clear ? PSTR("Upload and OTA URL overrides cleared.\n")
                                                  : PSTR("Upload and OTA URLs updated and saved.\n"));
      break;
  }
}
