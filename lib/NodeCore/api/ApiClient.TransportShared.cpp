#include "api/ApiClient.TransportShared.h"

#include <ESP8266WiFi.h>

#include <algorithm>
#include <cstring>
#include <strings.h>

#include "support/GatewayTargeting.h"
#include "net/NtpClient.h"
#include "support/Utils.h"

namespace ApiClientTransportShared {

EdgeGatewayTargets resolveEdgeGatewayTargets(ConfigManager& configManager) {
  const GatewayTargeting::GatewayPairIds ids = GatewayTargeting::resolvePreferredPair();

  EdgeGatewayTargets targets;
  copy_trunc(targets.primaryMdns, sizeof(targets.primaryMdns), configManager.getGatewayHost(ids.primary));
  copy_trunc(targets.primaryIp, sizeof(targets.primaryIp), configManager.getGatewayIp(ids.primary));
  copy_trunc(targets.secondaryMdns, sizeof(targets.secondaryMdns), configManager.getGatewayHost(ids.secondary));
  copy_trunc(targets.secondaryIp, sizeof(targets.secondaryIp), configManager.getGatewayIp(ids.secondary));
  configManager.releaseStrings();
  return targets;
}

void build_bearer(char* out, size_t out_len, const char* token, size_t token_len) {
  if (!out || out_len == 0) {
    return;
  }
  constexpr size_t kPrefixLen = sizeof("Bearer ") - 1;
  size_t n = (token_len < (out_len - 1 - kPrefixLen)) ? token_len : (out_len - 1 - kPrefixLen);
  memcpy_P(out, PSTR("Bearer "), kPrefixLen);
  if (n > 0) {
    memcpy(out + kPrefixLen, token, n);
  }
  out[kPrefixLen + n] = '\0';
}

PGM_P edge_target_label_P(size_t index) {
  switch (index) {
    case 0:
      return PSTR("mDNS primary");
    case 1:
      return PSTR("static primary");
    case 2:
      return PSTR("mDNS secondary");
    case 3:
      return PSTR("static secondary");
    default:
      return PSTR("unknown");
  }
}

bool build_auth_header_for_upload(char* out, size_t out_len, ConfigManager& configManager) {
  if (!out || out_len == 0) {
    return false;
  }
  const char* token = REDACTED
  const size_t token_len = REDACTED
  if (token_len =REDACTED
    out[0] = '\0';
    configManager.releaseStrings();
    return false;
  }
  build_bearer(out, out_len, token, token_len);
  configManager.releaseStrings();
  return true;
}

bool build_auth_header_for_ota(char* out, size_t out_len, ConfigManager& configManager) {
  if (!out || out_len == 0) {
    return false;
  }
  const char* token = REDACTED
  const size_t token_len = REDACTED
  if (token_len =REDACTED
    out[0] = '\0';
    configManager.releaseStrings();
    return false;
  }
  build_bearer(out, out_len, token, token_len);
  configManager.releaseStrings();
  return true;
}

void resolveCloudTarget(const char* url, char* host, size_t host_len, char* path, size_t path_len) {
  if (host && host_len > 0) {
    host[0] = '\0';
  }
  if (path && path_len > 0) {
    path[0] = '\0';
  }

  if (!url || url[0] == '\0') {
    if (host && host_len > 0) {
      copy_trunc_P(host, host_len, PSTR("example.com"));
    }
    if (path && path_len > 0) {
      copy_trunc_P(path, path_len, PSTR("/api/sensor"));
    }
    return;
  }

  const char* p = url;
  if (strncmp_P(p, PSTR("https://"), 8) == 0) {
    p += 8;
  } else if (strncmp_P(p, PSTR("http://"), 7) == 0) {
    p += 7;
  }

  const char* slash = strchr(p, '/');
  if (host && host_len > 0) {
    size_t len = slash ? static_cast<size_t>(slash - p) : strnlen(p, host_len - 1);
    len = std::min(len, host_len - 1);
    if (len > 0) {
      copy_trunc(host, host_len, p, len);
    } else {
      copy_trunc_P(host, host_len, PSTR("example.com"));
    }
  }

  if (path && path_len > 0) {
    if (slash) {
      copy_trunc(path, path_len, slash);
    } else {
      copy_trunc_P(path, path_len, PSTR("/api/sensor"));
    }
  }
}

bool read_line(WiFiClient& client, char* out, size_t out_len, unsigned long timeoutMs) {
  if (!out || out_len == 0) {
    return false;
  }
  size_t pos = 0;
  const unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (client.available()) {
      const char c = static_cast<char>(client.read());
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        out[pos] = '\0';
        return true;
      }
      if (pos + 1 < out_len) {
        out[pos++] = c;
      }
    }
    if (!client.connected() && !client.available()) {
      break;
    }
    yield();
  }
  out[pos] = '\0';
  return pos > 0;
}

StreamWriteResult write_all(WiFiClient& client, const uint8_t* data, size_t len, unsigned long timeoutMs) {
  StreamWriteResult result;
  if (!data || len == 0) {
    result.written = len;
    return result;
  }

  unsigned long lastProgressAt = millis();
  while (result.written < len) {
    if (!client.connected() && client.availableForWrite() <= 0) {
      result.disconnected = true;
      break;
    }

    const int available = client.availableForWrite();
    if (available <= 0) {
      if (millis() - lastProgressAt >= timeoutMs) {
        result.timedOut = true;
        break;
      }
      yield();
      continue;
    }

    const size_t remaining = len - result.written;
    const size_t chunk = std::min(remaining, static_cast<size_t>(available));
    const size_t n = client.write(data + result.written, chunk);
    if (n > 0) {
      result.written += n;
      lastProgressAt = millis();
      continue;
    }

    if (!client.connected()) {
      result.disconnected = true;
      break;
    }
    if (millis() - lastProgressAt >= timeoutMs) {
      result.timedOut = true;
      break;
    }
    yield();
  }

  return result;
}

int parse_status_code(const char* line) {
  if (!line) {
    return -1;
  }
  const char* p = strchr(line, ' ');
  if (!p) {
    return -1;
  }
  while (*p == ' ') {
    ++p;
  }
  int code = 0;
  int digits = 0;
  while (*p >= '0' && *p <= '9') {
    code = (code * 10) + (*p - '0');
    ++p;
    ++digits;
    if (digits >= 3) {
      break;
    }
  }
  return (digits >= 3) ? code : -1;
}

PGM_P lookup_http_reason_P(int code) {
  switch (code) {
    case 301:
      return PSTR("Moved Permanently");
    case 302:
      return PSTR("Redirect");
    case 303:
      return PSTR("See Other");
    case 307:
      return PSTR("Temp Redirect");
    case 308:
      return PSTR("Perm Redirect");
    case 400:
      return PSTR("Bad Request");
    case 401:
      return PSTR("REDACTED");
    case 403:
      return PSTR("Forbidden");
    case 404:
      return PSTR("Not Found");
    case 419:
      return PSTR("Session Expired");
    case 422:
      return PSTR("Unprocessable");
    case 429:
      return PSTR("Too Many Requests");
    case 500:
      return PSTR("Server Error");
    default:
      return PSTR("Error");
  }
}

void buildErrorMessageSimple(UploadResult& result) {
  if (result.success) {
    copy_trunc_P(result.message, sizeof(result.message), PSTR("OK"));
    return;
  }
  if (result.httpCode < 0) {
    result.message[0] = '\0';
    size_t pos = 0;
    pos = append_literal_P(result.message, sizeof(result.message), pos, PSTR("HTTP error "));
    pos = append_i32(result.message, sizeof(result.message), pos, result.httpCode);
    return;
  }
  copy_trunc_P(result.message, sizeof(result.message), lookup_http_reason_P(result.httpCode));
}

bool copy_header_value_if_matches(const char* line, const char* name, char* out, size_t out_len) {
  if (!line || !name || !out || out_len == 0) {
    return false;
  }
  const size_t name_len = strlen(name);
  if (strncasecmp(line, name, name_len) != 0 || line[name_len] != ':') {
    return false;
  }
  const char* value = line + name_len + 1;
  while (*value == ' ' || *value == '\t') {
    ++value;
  }
  copy_trunc(out, out_len, value);
  return true;
}

void parse_response_headers(
    WiFiClient& client, char* dateBuf, size_t dateBufLen, char* locationBuf, size_t locationBufLen) {
  char line[128];
  while (read_line(client, line, sizeof(line), 5000)) {
    if (line[0] == '\0') {
      break;
    }
    if (dateBuf && dateBuf[0] == '\0') {
      (void)copy_header_value_if_matches(line, "Date", dateBuf, dateBufLen);
    }
    if (locationBuf && locationBuf[0] == '\0') {
      (void)copy_header_value_if_matches(line, "Location", locationBuf, locationBufLen);
    }
  }
}

void sync_time_from_http_date(NtpClient& ntpClient, const char* dateBuf) {
  if (!dateBuf || dateBuf[0] == '\0' || ntpClient.isTimeSynced()) {
    return;
  }
  const time_t serverTime = Utils::parse_http_date(dateBuf);
  if (serverTime > 0) {
    ntpClient.setManualTime(serverTime);
  }
}

void copy_location_display(char* out, size_t out_len, const char* location) {
  if (!out || out_len == 0) {
    return;
  }
  out[0] = '\0';
  if (!location || location[0] == '\0') {
    return;
  }
  const size_t len = strnlen(location, 64);
  const size_t prefix_len = (out_len > 4) ? std::min<size_t>(out_len - 4, 17) : 0;
  if (prefix_len > 0 && len > prefix_len) {
    copy_trunc(out, out_len, location, prefix_len);
    copy_trunc_P(out + prefix_len, out_len - prefix_len, PSTR("..."));
    return;
  }
  copy_trunc(out, out_len, location, len);
}

size_t read_body_preview(WiFiClient& client, char* out, size_t out_len, unsigned long timeoutMs) {
  if (!out || out_len == 0) {
    return 0;
  }
  out[0] = '\0';
  size_t pos = 0;
  const unsigned long start = millis();
  while ((millis() - start) < timeoutMs && pos + 1 < out_len) {
    while (client.available() && pos + 1 < out_len) {
      out[pos++] = static_cast<char>(client.read());
    }
    if (!client.connected() && !client.available()) {
      break;
    }
    yield();
  }
  out[pos] = '\0';
  return pos;
}

bool response_body_indicates_waf_block(const char* body) {
  if (!body || body[0] == '\0') {
    return false;
  }
  return strstr(body, "Access denied by Imunify360") != nullptr ||
         strstr(body, "bot-protection") != nullptr ||
         strstr(body, "automation should be whitelisted") != nullptr;
}

}  // namespace ApiClientTransportShared
