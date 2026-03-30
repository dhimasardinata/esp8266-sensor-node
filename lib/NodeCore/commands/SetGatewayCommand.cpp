#include "SetGatewayCommand.h"

#include <IPAddress.h>
#include <cstring>
#include <strings.h>

#include "system/ConfigManager.h"
#include "support/Utils.h"

namespace {
  enum class GatewayId : uint8_t { Gh1 = 1, Gh2 = 2 };
  enum class Field : uint8_t { Both, Host, Ip };
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

  bool parseGatewayId(const char* token, GatewayId& out) {
    if (strcasecmp(token, "gh1") =REDACTED
      out = GatewayId::Gh1;
      return true;
    }
    if (strcasecmp(token, "gh2") =REDACTED
      out = GatewayId::Gh2;
      return true;
    }
    return false;
  }

  bool parseField(const char* token, Field& out) {
    if (strcasecmp(token, "host") =REDACTED
      out = Field::Host;
      return true;
    }
    if (strcasecmp(token, "ip") =REDACTED
      out = Field::Ip;
      return true;
    }
    return false;
  }

  const char* defaultGatewayHost(GatewayId gh) {
    return (gh == GatewayId::Gh2) ? DEFAULT_GATEWAY_HOST_GH2 : DEFAULT_GATEWAY_HOST_GH1;
  }

  const char* defaultGatewayIp(GatewayId gh) {
    return (gh == GatewayId::Gh2) ? DEFAULT_GATEWAY_IP_GH2 : DEFAULT_GATEWAY_IP_GH1;
  }

  bool isValidHostValue(const char* host) {
    if (!host || host[0] == '\0') {
      return false;
    }
    if (strstr(host, "://") != nullptr || strchr(host, '/') != nullptr || strchr(host, '\\') != nullptr ||
        strchr(host, ':') != nullptr) {
      return false;
    }
    for (const char* p = host; *p != '\0'; ++p) {
      const unsigned char c = static_cast<unsigned char>(*p);
      if (c <= 32 || c > 126) {
        return false;
      }
    }
    return true;
  }

  bool isValidIpValue(const char* ipText) {
    if (!ipText || ipText[0] == '\0') {
      return false;
    }
    IPAddress ip;
    return ip.fromString(ipText);
  }

  void printGatewayState(AsyncWebSocketClient* client,
                         ConfigManager& configManager,
                         GatewayId gateway,
                         Field field) {
    const uint8_t ghId = static_cast<uint8_t>(gateway);
    const char* host = configManager.getGatewayHost(ghId);
    const char* ip = configManager.getGatewayIp(ghId);
    const char* displayHost = (host && host[0] != '\0') ? host : "<none>";
    const char* displayIp = (ip && ip[0] != '\0') ? ip : "<none>";

    if (field == Field::Both) {
      Utils::ws_printf_P(client, PSTR("Configured GH%u Host: %s\n"), ghId, displayHost);
      Utils::ws_printf_P(client, PSTR("Configured GH%u IP: %s\n"), ghId, displayIp);
    } else if (field == Field::Host) {
      Utils::ws_printf_P(client, PSTR("Configured GH%u Host: %s\n"), ghId, displayHost);
    } else {
      Utils::ws_printf_P(client, PSTR("Configured GH%u IP: %s\n"), ghId, displayIp);
    }
  }
}  // namespace

SetGatewayCommand::SetGatewayCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void SetGatewayCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) {
    return;
  }

  const char* args = context.args ? skipSpaces(context.args) : "";
  if (*args == '\0') {
    Utils::ws_printf_P(
        context.client,
        PSTR("[ERROR] Usage: setgateway [gh1|gh2] [host|ip] <value|default|none|show>\n"));
    return;
  }

  GatewayId gateway = GatewayId::Gh1;
  Field field = Field::Both;
  Action action = Action::SetValue;

  char first[10] = {0};
  char second[10] = {0};
  const char* rest = nextToken(args, first, sizeof(first));

  if (strcasecmp(first, "show") == 0) {
    printGatewayState(context.client, m_configManager, GatewayId::Gh1, Field::Both);
    printGatewayState(context.client, m_configManager, GatewayId::Gh2, Field::Both);
    m_configManager.releaseStrings();
    return;
  }

  if (!parseGatewayId(first, gateway)) {
    Utils::ws_printf_P(context.client,
                       PSTR("[ERROR] First argument must be gh1, gh2, or show.\n"));
    return;
  }

  rest = nextToken(rest, second, sizeof(second));
  if (second[0] == '\0') {
    Utils::ws_printf_P(
        context.client,
        PSTR("[ERROR] Usage: setgateway [gh1|gh2] [host|ip] <value|default|none|show>\n"));
    return;
  }

  if (strcasecmp(second, "show") == 0) {
    printGatewayState(context.client, m_configManager, gateway, Field::Both);
    m_configManager.releaseStrings();
    return;
  }

  if (!parseField(second, field)) {
    Utils::ws_printf_P(context.client,
                       PSTR("[ERROR] Second argument must be host, ip, or show.\n"));
    return;
  }

  const char* value = skipSpaces(rest);
  if (*value == '\0') {
    Utils::ws_printf_P(
        context.client,
        PSTR("[ERROR] Usage: setgateway [gh1|gh2] [host|ip] <value|default|none|show>\n"));
    return;
  }

  if (strcasecmp(value, "show") == 0) {
    action = Action::Show;
  } else if (strcasecmp(value, "default") == 0 || strcasecmp(value, "defaults") == 0) {
    action = Action::UseDefault;
  } else if (strcasecmp(value, "none") == 0 || strcasecmp(value, "clear") == 0) {
    action = Action::Clear;
  }

  if (action == Action::Show) {
    printGatewayState(context.client, m_configManager, gateway, field);
    m_configManager.releaseStrings();
    return;
  }

  if (action == Action::SetValue) {
    if (field == Field::Host) {
      if (strnlen(value, MAX_GATEWAY_HOST_LEN + 1) > MAX_GATEWAY_HOST_LEN - 1) {
        Utils::ws_printf_P(context.client, PSTR("[ERROR] Host too long (max %u chars).\n"),
                           static_cast<unsigned>(MAX_GATEWAY_HOST_LEN - 1));
        return;
      }
      if (!isValidHostValue(value)) {
        Utils::ws_printf_P(context.client,
                           PSTR("[ERROR] Invalid host. Use bare hostname only (no protocol/path/port).\n"));
        return;
      }
    } else {
      if (strnlen(value, MAX_GATEWAY_IP_LEN + 1) > MAX_GATEWAY_IP_LEN - 1) {
        Utils::ws_printf_P(context.client, PSTR("[ERROR] IP too long (max %u chars).\n"),
                           static_cast<unsigned>(MAX_GATEWAY_IP_LEN - 1));
        return;
      }
      if (!isValidIpValue(value)) {
        Utils::ws_printf_P(context.client, PSTR("[ERROR] Invalid IPv4 address.\n"));
        return;
      }
    }
  }

  const uint8_t ghId = static_cast<uint8_t>(gateway);
  if (field == Field::Host) {
    if (action == Action::UseDefault) {
      m_configManager.setGatewayHost(ghId, defaultGatewayHost(gateway));
    } else if (action == Action::Clear) {
      m_configManager.setGatewayHost(ghId, "");
    } else {
      m_configManager.setGatewayHost(ghId, value);
    }
  } else if (field == Field::Ip) {
    if (action == Action::UseDefault) {
      m_configManager.setGatewayIp(ghId, defaultGatewayIp(gateway));
    } else if (action == Action::Clear) {
      m_configManager.setGatewayIp(ghId, "");
    } else {
      m_configManager.setGatewayIp(ghId, value);
    }
  }

  const ConfigStatus status = m_configManager.save();
  m_configManager.releaseStrings();
  if (status != ConfigStatus::OK) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Failed to save gateway setting.\n"));
    return;
  }

  const char* fieldName = (field == Field::Host) ? "host" : "IP";
  if (action == Action::UseDefault) {
    Utils::ws_printf_P(context.client, PSTR("GH%u %s restored to default.\n"), ghId, fieldName);
  } else if (action == Action::Clear) {
    Utils::ws_printf_P(context.client, PSTR("GH%u %s cleared.\n"), ghId, fieldName);
  } else {
    Utils::ws_printf_P(context.client, PSTR("GH%u %s updated and saved.\n"), ghId, fieldName);
  }
}
