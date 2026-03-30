#include "CacheStatusCommand.h"

#include <cstdio>
#include <cstring>
#include <stdint.h>

#include "api/ApiClient.h"
#include "storage/CacheManager.h"  // Concrete type for CRTP
#include "system/ConfigManager.h"
#include "storage/RtcManager.h"
#include "support/Utils.h"

namespace {
  constexpr uint32_t kWorstCaseLittleFsRecordBytes =
      sizeof(uint16_t) + sizeof(uint16_t) + MAX_PAYLOAD_SIZE + sizeof(uint32_t);

  void formatDuration(char* out, size_t out_len, uint64_t totalMs) {
    if (!out || out_len == 0) {
      return;
    }
    if (totalMs =REDACTED
      strncpy(out, "0m", out_len - 1);
      out[out_len - 1] = '\0';
      return;
    }

    uint64_t totalSeconds = REDACTED
    uint64_t totalMinutes = REDACTED
    uint64_t totalHours = REDACTED
    uint64_t days = totalHours / 24ULL;
    uint64_t hours = totalHours % 24ULL;
    uint64_t minutes = totalMinutes % 60ULL;

    if (days > 0) {
      snprintf(out, out_len, "%lud %luh", static_cast<unsigned long>(days), static_cast<unsigned long>(hours));
    } else if (totalHours > 0) {
      snprintf(out, out_len, "%luh %lum", static_cast<unsigned long>(hours), static_cast<unsigned long>(minutes));
    } else {
      snprintf(out, out_len, "%lum", static_cast<unsigned long>(totalMinutes > 0 ? totalMinutes : REDACTED
    }
  }
}  // namespace

CacheStatusCommand::CacheStatusCommand(CacheManager& cacheManager,
                                       ApiClient& apiClient,
                                       ConfigManager& configManager)
    : m_cacheManager(cacheManager), m_apiClient(apiClient), m_configManager(configManager) {}

void CacheStatusCommand::execute(const CommandContext& context) {
  uint32_t size_bytes, head, tail;
  m_cacheManager.get_status(size_bytes, head, tail);

  if (!context.client || !context.client->canSend()) {
    return;
  }

  const uint32_t sampleIntervalMs = m_configManager.getConfig().SENSOR_SAMPLE_INTERVAL_MS;
  const uint32_t rtcCount = static_cast<uint32_t>(RtcManager::getCount());
  const uint32_t rtcRemaining = (rtcCount < RTC_MAX_RECORDS) ? (RTC_MAX_RECORDS - rtcCount) : 0;
  const uint32_t littleFsUsedRecordsMin = size_bytes / kWorstCaseLittleFsRecordBytes;
  const uint32_t littleFsRemainingBytes = (size_bytes < MAX_CACHE_DATA_SIZE) ? (MAX_CACHE_DATA_SIZE - size_bytes) : 0;
  const uint32_t littleFsRemainingRecordsMin = littleFsRemainingBytes / kWorstCaseLittleFsRecordBytes;
  char rtcHoldLeft[16];
  char littleFsHoldLeft[16];
  char rtcHoldNow[16];
  char littleFsHoldNow[16];
  char totalHoldNow[16];
  char totalHoldLeft[16];
  const uint64_t rtcHoldNowMs = static_cast<uint64_t>(rtcCount) * sampleIntervalMs;
  const uint64_t littleFsHoldNowMs = static_cast<uint64_t>(littleFsUsedRecordsMin) * sampleIntervalMs;
  const uint64_t totalHoldNowMs = REDACTED
  const uint64_t rtcHoldLeftMs = static_cast<uint64_t>(rtcRemaining) * sampleIntervalMs;
  const uint64_t littleFsHoldLeftMs = static_cast<uint64_t>(littleFsRemainingRecordsMin) * sampleIntervalMs;
  const uint64_t totalHoldLeftMs = REDACTED
  formatDuration(rtcHoldNow, sizeof(rtcHoldNow), rtcHoldNowMs);
  formatDuration(littleFsHoldNow, sizeof(littleFsHoldNow), littleFsHoldNowMs);
  formatDuration(totalHoldNow, sizeof(totalHoldNow), totalHoldNowMs);
  formatDuration(rtcHoldLeft, sizeof(rtcHoldLeft), static_cast<uint64_t>(rtcRemaining) * sampleIntervalMs);
  formatDuration(littleFsHoldLeft, sizeof(littleFsHoldLeft), static_cast<uint64_t>(littleFsRemainingRecordsMin) * sampleIntervalMs);
  formatDuration(totalHoldLeft, sizeof(totalHoldLeft), totalHoldLeftMs);

  Utils::ws_printf_P(context.client, PSTR("Cache Status:\n"));
  Utils::ws_printf_P(context.client,
                     PSTR("  RTC: %u/%u records\n"),
                     static_cast<unsigned>(RtcManager::getCount()),
                     static_cast<unsigned>(RTC_MAX_RECORDS));
  Utils::ws_printf_P(context.client,
                     PSTR("  LittleFS: %lu/%lu bytes\n"),
                     static_cast<unsigned long>(size_bytes),
                     static_cast<unsigned long>(MAX_CACHE_DATA_SIZE));
  Utils::ws_printf_P(context.client,
                     PSTR("  Emergency: %u/%u | Backpressure: %s\n"),
                     static_cast<unsigned>(m_apiClient.getEmergencyQueueDepth()),
                     static_cast<unsigned>(m_apiClient.getEmergencyQueueCapacity()),
                     m_apiClient.isEmergencyBackpressureActive() ? "ON" : "OFF");
  Utils::ws_printf_P(context.client,
                     PSTR("  Current Hold: RTC ~%s | LittleFS >= %s | Total >= %s\n"),
                     rtcHoldNow,
                     littleFsHoldNow,
                     totalHoldNow);
  Utils::ws_printf_P(context.client,
                     PSTR("  Remaining Hold: RTC ~%s | LittleFS >= %s | Total >= %s\n"),
                     rtcHoldLeft,
                     littleFsHoldLeft,
                     totalHoldLeft);
  Utils::ws_printf_P(context.client,
                     PSTR("  Estimate Basis: sample=%lu ms | worst-case record=%u B\n"),
                     static_cast<unsigned long>(sampleIntervalMs),
                     static_cast<unsigned>(kWorstCaseLittleFsRecordBytes));
  Utils::ws_printf_P(context.client, PSTR("  Head: %u\n"), head);
  Utils::ws_printf_P(context.client, PSTR("  Tail: %u\n"), tail);
}
