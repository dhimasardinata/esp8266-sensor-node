#include "api/ApiClient.UploadShared.h"

#include <ESP8266WiFi.h>

#include <cstring>

#include "support/GatewayTargeting.h"

namespace ApiClientUploadShared {

void copy_default_datetime(char* out, size_t out_len) {
  copy_trunc_P(out, out_len, PSTR("1970-01-01 00:00:00"));
}

bool append_http_prefix(char* out, size_t out_len, size_t& pos) {
  return append_bytes_strict_P(out, out_len, pos, PSTR("http://"));
}

bool build_gateway_url_from_host_str(char* out, size_t out_len, const char* host, const char* path) {
  if (!out || out_len == 0 || !host || !path) {
    return false;
  }
  if (host[0] == '\0') {
    return false;
  }
  size_t pos = 0;
  if (!append_http_prefix(out, out_len, pos)) {
    return false;
  }
  pos = append_cstr(out, out_len, pos, host);
  if (pos == 0 || pos >= out_len) {
    return false;
  }
  return append_bytes_strict(out, out_len, pos, path, strnlen(path, out_len - pos - 1));
}

bool build_gateway_url_from_ip_str(char* out, size_t out_len, const char* ip, const char* path) {
  return build_gateway_url_from_host_str(out, out_len, ip, path);
}

bool append_gateway_candidate(char urls[][MAX_URL_LEN], size_t max_urls, size_t& count, const char* candidate) {
  if (!urls || max_urls == 0 || !candidate || candidate[0] == '\0') {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    if (strncmp(urls[i], candidate, MAX_URL_LEN) == 0) {
      return false;
    }
  }
  if (count >= max_urls) {
    return false;
  }
  const size_t n = strnlen(candidate, MAX_URL_LEN - 1);
  memcpy(urls[count], candidate, n);
  urls[count][n] = '\0';
  ++count;
  return true;
}

size_t build_gateway_url_candidates(
    char urls[][MAX_URL_LEN], size_t max_urls, ConfigManager& configManager, const char* path) {
  if (!urls || max_urls == 0 || !path) {
    return 0;
  }
  for (size_t i = 0; i < max_urls; ++i) {
    urls[i][0] = '\0';
  }

  size_t count = 0;
  const GatewayTargeting::GatewayPairIds ids = GatewayTargeting::resolvePreferredPair();
  char primaryMdnsHost[MAX_GATEWAY_HOST_LEN] = {0};
  char secondaryMdnsHost[MAX_GATEWAY_HOST_LEN] = {0};
  char primaryIp[MAX_GATEWAY_IP_LEN] = {0};
  char secondaryIp[MAX_GATEWAY_IP_LEN] = {0};
  copy_trunc(primaryMdnsHost, sizeof(primaryMdnsHost), configManager.getGatewayHost(ids.primary));
  copy_trunc(secondaryMdnsHost, sizeof(secondaryMdnsHost), configManager.getGatewayHost(ids.secondary));
  copy_trunc(primaryIp, sizeof(primaryIp), configManager.getGatewayIp(ids.primary));
  copy_trunc(secondaryIp, sizeof(secondaryIp), configManager.getGatewayIp(ids.secondary));
  configManager.releaseStrings();

  char mdnsUrl[MAX_URL_LEN] = {0};
  if (build_gateway_url_from_host_str(mdnsUrl, sizeof(mdnsUrl), primaryMdnsHost, path)) {
    (void)append_gateway_candidate(urls, max_urls, count, mdnsUrl);
  }

  char ipUrl[MAX_URL_LEN] = {0};
  if (build_gateway_url_from_ip_str(ipUrl, sizeof(ipUrl), primaryIp, path)) {
    (void)append_gateway_candidate(urls, max_urls, count, ipUrl);
  }

  char secondaryMdnsUrl[MAX_URL_LEN] = {0};
  if (build_gateway_url_from_host_str(secondaryMdnsUrl, sizeof(secondaryMdnsUrl), secondaryMdnsHost, path)) {
    (void)append_gateway_candidate(urls, max_urls, count, secondaryMdnsUrl);
  }

  if (build_gateway_url_from_ip_str(ipUrl, sizeof(ipUrl), secondaryIp, path)) {
    (void)append_gateway_candidate(urls, max_urls, count, ipUrl);
  }
  return count;
}

bool build_gateway_url(char* out, size_t out_len, ConfigManager& configManager, const char* path) {
  if (!out || out_len == 0 || !path) {
    return false;
  }
  out[0] = '\0';
  char candidates[4][MAX_URL_LEN] = {{0}};
  const size_t count = build_gateway_url_candidates(candidates, 4, configManager, path);
  if (count == 0) {
    return false;
  }

  const size_t n = strnlen(candidates[0], out_len - 1);
  memcpy(out, candidates[0], n);
  out[n] = '\0';
  return n > 0;
}

bool ssid_equals(const char* a, const char* b) {
  if (!a || !b) {
    return false;
  }
  return strncmp(a, b, WIFI_SSID_MAX_LEN) =REDACTED
}

int16_t resolve_nonactive_rssi(WifiManager& wifiManager) {
  auto& store = wifiManager.getCredentialStore();
  const WifiCredential* primary = REDACTED
  const WifiCredential* secondary = REDACTED
  if (!primary || !secondary) {
    return -100;
  }

  char activeSsid[WIFI_SSID_MAX_LEN] = REDACTED
  if (WiFi.isConnected()) {
    WiFi.SSID().toCharArray(activeSsid, sizeof(activeSsid));
  }

  if (ssid_equals(activeSsid, primary->ssid)) {
    return secondary->lastRssi;
  }
  if (ssid_equals(activeSsid, secondary->ssid)) {
    return primary->lastRssi;
  }
  return -100;
}

bool extract_recorded_at_value(const char* payload, char* out, size_t out_len, size_t& value_len) {
  if (!payload || !out || out_len == 0) {
    return false;
  }
  char recordedPrefix[sizeof(",\"recorded_at\":\"")];
  strcpy_P(recordedPrefix, PSTR(",\"recorded_at\":\""));
  const char* start = strstr(payload, recordedPrefix);
  if (!start) {
    return false;
  }

  const char* valueStart = start + (sizeof(",\"recorded_at\":\"") - 1);
  const char* endQuote = strchr(valueStart, '"');
  if (!endQuote) {
    return false;
  }

  const size_t len = static_cast<size_t>(endQuote - valueStart);
  if (len >= out_len) {
    return false;
  }

  if (len > 0) {
    memcpy(out, valueStart, len);
  }
  out[len] = '\0';
  value_len = len;
  return true;
}

bool strip_recorded_at_field(char* payload, size_t& len) {
  if (!payload || len == 0) {
    return false;
  }
  char recordedPrefix[sizeof(",\"recorded_at\":\"")];
  strcpy_P(recordedPrefix, PSTR(",\"recorded_at\":\""));
  char* start = strstr(payload, recordedPrefix);
  if (!start) {
    return true;
  }

  char* valueStart = start + (sizeof(",\"recorded_at\":\"") - 1);
  char* endQuote = strchr(valueStart, '"');
  if (!endQuote) {
    return false;
  }

  char* removeEnd = endQuote + 1;
  const size_t removeLen = static_cast<size_t>(removeEnd - start);
  const size_t tailLen = strnlen(removeEnd, len - static_cast<size_t>(removeEnd - payload)) + 1;
  memmove(start, removeEnd, tailLen);
  if (len >= removeLen) {
    len -= removeLen;
  } else {
    len = strnlen(payload, MAX_PAYLOAD_SIZE);
  }
  return true;
}

size_t buildSensorPayload(char* out,
                          size_t out_len,
                          uint32_t gh_id,
                          uint32_t node_id,
                          int32_t temp10,
                          int32_t hum10,
                          uint32_t lux,
                          int32_t rssi,
                          const char* timeStr,
                          size_t timeLen) {
  if (!out || out_len == 0 || !timeStr) {
    return 0;
  }
  temp10 = SensorNormalization::clampTemperatureTenths(temp10);
  hum10 = SensorNormalization::clampHumidityTenths(hum10);
  lux = SensorNormalization::clampLightUInt(lux);
  out[0] = '\0';
  size_t pos = 0;
  if (!append_bytes_strict_P(out, out_len, pos, PSTR("{\"gh_id\":"))) {
    return 0;
  }
  if (!append_u32_strict(out, out_len, pos, gh_id)) {
    return 0;
  }
  if (!append_bytes_strict_P(out, out_len, pos, PSTR(",\"node_id\":"))) {
    return 0;
  }
  if (!append_u32_strict(out, out_len, pos, node_id)) {
    return 0;
  }
  if (!append_bytes_strict_P(out, out_len, pos, PSTR(",\"temperature\":"))) {
    return 0;
  }
  if (!append_fixed1_strict(out, out_len, pos, temp10)) {
    return 0;
  }
  if (!append_bytes_strict_P(out, out_len, pos, PSTR(",\"humidity\":"))) {
    return 0;
  }
  if (!append_fixed1_strict(out, out_len, pos, hum10)) {
    return 0;
  }
  if (!append_bytes_strict_P(out, out_len, pos, PSTR(",\"light_intensity\":"))) {
    return 0;
  }
  if (!append_u32_strict(out, out_len, pos, lux)) {
    return 0;
  }
  if (!append_bytes_strict_P(out, out_len, pos, PSTR(",\"rssi\":"))) {
    return 0;
  }
  if (!append_i32_strict(out, out_len, pos, rssi)) {
    return 0;
  }
  if (!append_bytes_strict_P(out, out_len, pos, PSTR(",\"recorded_at\":\""))) {
    return 0;
  }
  if (!append_bytes_strict(out, out_len, pos, timeStr, timeLen)) {
    return 0;
  }
  if (!append_bytes_strict_P(out, out_len, pos, PSTR("\"}"))) {
    return 0;
  }
  return pos;
}

void format_datetime(char* out, size_t out_len, const tm& t) {
  if (!out || out_len < 20) {
    if (out && out_len > 0) {
      out[0] = '\0';
    }
    return;
  }
  const uint16_t year = static_cast<uint16_t>(t.tm_year + 1900);
  const uint8_t mon = static_cast<uint8_t>(t.tm_mon + 1);
  const uint8_t day = static_cast<uint8_t>(t.tm_mday);
  const uint8_t hour = static_cast<uint8_t>(t.tm_hour);
  const uint8_t min = static_cast<uint8_t>(t.tm_min);
  const uint8_t sec = static_cast<uint8_t>(t.tm_sec);

  out[0] = static_cast<char>('0' + (year / 1000) % 10);
  out[1] = static_cast<char>('0' + (year / 100) % 10);
  out[2] = static_cast<char>('0' + (year / 10) % 10);
  out[3] = static_cast<char>('0' + (year % 10));
  out[4] = '-';
  out[5] = static_cast<char>('0' + (mon / 10));
  out[6] = static_cast<char>('0' + (mon % 10));
  out[7] = '-';
  out[8] = static_cast<char>('0' + (day / 10));
  out[9] = static_cast<char>('0' + (day % 10));
  out[10] = ' ';
  out[11] = static_cast<char>('0' + (hour / 10));
  out[12] = static_cast<char>('0' + (hour % 10));
  out[13] = ':';
  out[14] = static_cast<char>('0' + (min / 10));
  out[15] = static_cast<char>('0' + (min % 10));
  out[16] = ':';
  out[17] = static_cast<char>('0' + (sec / 10));
  out[18] = static_cast<char>('0' + (sec % 10));
  out[19] = '\0';
}

void format_record_timestamp(uint32_t timestamp, char* out, size_t out_len) {
  copy_default_datetime(out, out_len);
  if (timestamp <= NTP_VALID_TIMESTAMP_THRESHOLD) {
    return;
  }

  tm timeinfo;
  const time_t ts = static_cast<time_t>(timestamp);
  localtime_r(&ts, &timeinfo);
  format_datetime(out, out_len, timeinfo);
}

bool build_payload_from_record_fields(char* out,
                                      size_t out_len,
                                      uint32_t timestamp,
                                      int32_t temp10,
                                      int32_t hum10,
                                      uint32_t lux,
                                      int32_t rssi,
                                      size_t& payload_len) {
  char timeBuf[20];
  format_record_timestamp(timestamp, timeBuf, sizeof(timeBuf));
  payload_len = buildSensorPayload(out,
                                   out_len,
                                   static_cast<uint32_t>(GH_ID),
                                   static_cast<uint32_t>(NODE_ID),
                                   temp10,
                                   hum10,
                                   lux,
                                   rssi,
                                   timeBuf,
                                   19);
  if (payload_len == 0) {
    return false;
  }
  out[payload_len] = '\0';
  return true;
}

bool build_payload_from_rtc_record(
    char* out, size_t out_len, const RtcSensorRecord& record, size_t& payload_len) {
  return build_payload_from_record_fields(out,
                                          out_len,
                                          record.timestamp,
                                          record.temp10,
                                          record.hum10,
                                          static_cast<uint32_t>(record.lux),
                                          static_cast<int32_t>(record.rssi),
                                          payload_len);
}

}  // namespace ApiClientUploadShared
