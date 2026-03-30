#pragma once

// ApiClient.UploadShared.h - internal upload helper utilities shared across upload modules

#include <Arduino.h>
#include <cstddef>

#include "system/ConfigManager.h"
#include "storage/RtcManager.h"
#include "sensor/SensorNormalization.h"
#include "support/TextBufferUtils.h"
#include "REDACTED"

namespace ApiClientUploadShared {
  using TextBufferUtils::append_bytes_strict;
  using TextBufferUtils::append_bytes_strict_P;
  using TextBufferUtils::append_char_strict;
  using TextBufferUtils::append_cstr;
  using TextBufferUtils::append_fixed1_strict;
  using TextBufferUtils::append_i32;
  using TextBufferUtils::append_i32_strict;
  using TextBufferUtils::append_literal;
  using TextBufferUtils::append_literal_P;
  using TextBufferUtils::append_u32;
  using TextBufferUtils::append_u32_strict;
  using TextBufferUtils::copy_trunc;
  using TextBufferUtils::copy_trunc_P;
  using TextBufferUtils::u32_to_dec;

  void copy_default_datetime(char* out, size_t out_len);
  bool append_http_prefix(char* out, size_t out_len, size_t& pos);
  bool build_gateway_url_from_host_str(char* out, size_t out_len, const char* host, const char* path);
  bool build_gateway_url_from_ip_str(char* out, size_t out_len, const char* ip, const char* path);
  bool append_gateway_candidate(char urls[][MAX_URL_LEN], size_t max_urls, size_t& count, const char* candidate);
  size_t build_gateway_url_candidates(
      char urls[][MAX_URL_LEN], size_t max_urls, ConfigManager& configManager, const char* path);
  bool build_gateway_url(char* out, size_t out_len, ConfigManager& configManager, const char* path);
  bool ssid_equals(const char* a, const char* b);
  int16_t resolve_nonactive_rssi(WifiManager& wifiManager);
  bool extract_recorded_at_value(const char* payload, char* out, size_t out_len, size_t& value_len);
  bool strip_recorded_at_field(char* payload, size_t& len);
  size_t buildSensorPayload(char* out,
                            size_t out_len,
                            uint32_t gh_id,
                            uint32_t node_id,
                            int32_t temp10,
                            int32_t hum10,
                            uint32_t lux,
                            int32_t rssi,
                            const char* timeStr,
                            size_t timeLen);
  void format_datetime(char* out, size_t out_len, const tm& t);
  void format_record_timestamp(uint32_t timestamp, char* out, size_t out_len);
  bool build_payload_from_record_fields(char* out,
                                        size_t out_len,
                                        uint32_t timestamp,
                                        int32_t temp10,
                                        int32_t hum10,
                                        uint32_t lux,
                                        int32_t rssi,
                                        size_t& payload_len);
  bool build_payload_from_rtc_record(
      char* out, size_t out_len, const RtcSensorRecord& record, size_t& payload_len);
}  // namespace ApiClientUploadShared
