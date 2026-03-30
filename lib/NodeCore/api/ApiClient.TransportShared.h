#pragma once

// ApiClient.TransportShared.h - internal transport helper utilities shared across transport modules

#include <Arduino.h>
#include <cstddef>

#include "api/ApiClient.State.h"
#include "system/ConfigManager.h"
#include "support/TextBufferUtils.h"

class NtpClient;
class WiFiClient;

// ApiClient.Transport.cpp - HTTP transport and state machine

namespace ApiClientTransportShared {
  static constexpr size_t kMaxDynamicTokenLen = REDACTED
  static constexpr size_t kBearerHeaderBufferLen = 192;

  struct EdgeGatewayTargets {
    char primaryMdns[MAX_GATEWAY_HOST_LEN] = {0};
    char primaryIp[MAX_GATEWAY_IP_LEN] = {0};
    char secondaryMdns[MAX_GATEWAY_HOST_LEN] = {0};
    char secondaryIp[MAX_GATEWAY_IP_LEN] = {0};
  };

  using TextBufferUtils::append_i32;
  using TextBufferUtils::append_literal;
  using TextBufferUtils::append_literal_P;
  using TextBufferUtils::append_u32;
  using TextBufferUtils::copy_trunc;
  using TextBufferUtils::copy_trunc_P;
  using TextBufferUtils::u32_to_dec;

  EdgeGatewayTargets resolveEdgeGatewayTargets(ConfigManager& configManager);
  void build_bearer(char* out, size_t out_len, const char* token, size_t token_len);
  PGM_P edge_target_label_P(size_t index);
  bool build_auth_header_for_upload(char* out, size_t out_len, ConfigManager& configManager);
  bool build_auth_header_for_ota(char* out, size_t out_len, ConfigManager& configManager);
  void resolveCloudTarget(const char* url, char* host, size_t host_len, char* path, size_t path_len);
  bool read_line(WiFiClient& client, char* out, size_t out_len, unsigned long timeoutMs);

  struct StreamWriteResult {
    size_t written = 0;
    bool timedOut = false;
    bool disconnected = false;
  };

  StreamWriteResult write_all(WiFiClient& client, const uint8_t* data, size_t len, unsigned long timeoutMs);
  int parse_status_code(const char* line);
  PGM_P lookup_http_reason_P(int code);
  void buildErrorMessageSimple(UploadResult& result);
  bool copy_header_value_if_matches(const char* line, const char* name, char* out, size_t out_len);
  void parse_response_headers(
      WiFiClient& client, char* dateBuf, size_t dateBufLen, char* locationBuf, size_t locationBufLen);
  void sync_time_from_http_date(NtpClient& ntpClient, const char* dateBuf);
  void copy_location_display(char* out, size_t out_len, const char* location);
  size_t read_body_preview(WiFiClient& client, char* out, size_t out_len, unsigned long timeoutMs);
  bool response_body_indicates_waf_block(const char* body);
}  // namespace ApiClientTransportShared
