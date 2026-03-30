#include "net/NtpClient.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <sys/time.h>  // Required for settimeofday
#include <time.h>

#include "system/ConfigManager.h"  // For NTP_VALID_TIMESTAMP_THRESHOLD
#include "support/CryptoUtils.h"
#include "system/Logger.h"
#include "storage/Paths.h"
#include "REDACTED"

NtpClient::NtpClient(WifiManager& wifiManager) : m_wifiManager(wifiManager) {}

void NtpClient::init() {
  m_retryTimer.setInterval(AppConstants::NTP_INITIAL_DELAY_MS);
  m_syncTimeoutTimer.setInterval(AppConstants::NTP_SYNC_TIMEOUT_MS);
  loadSavedTime();
}

void NtpClient::onWifiStateChanged(WifiManager::State newState) {
  if (newState == WifiManager::State::CONNECTED_STA) {
    LOG_INFO("REDACTED", F("REDACTED"));
    m_ntpVerified = false;  // Force sync requirement.
    m_sync_in_progress = false;
    m_retryTimer.reset();  // Ensure timer elapses in next loop.
    startSync();
  }
}

void NtpClient::handle() {
  if (m_wifiManager.getState() != REDACTED

  if (!m_ntpVerified && !m_sync_in_progress && m_retryTimer.hasElapsed()) startSync();
  if (m_sync_in_progress) checkSyncStatus();
}

bool NtpClient::isTimeSynced() const noexcept {
  return m_isSynced;
}

unsigned long NtpClient::getLastSyncMillis() const noexcept {
  return m_lastSuccessMillis;
}

NtpClient::TimeSource NtpClient::getTimeSource() const noexcept {
  return m_timeSource;
}

void NtpClient::copyTimeSourceLabel(char* out, size_t out_len) const noexcept {
  if (!out || out_len == 0) {
    return;
  }
  PGM_P label = PSTR("NONE");
  switch (m_timeSource) {
    case TimeSource::NTP:
      label = PSTR("NTP");
      break;
    case TimeSource::MANUAL:
      label = PSTR("MANUAL");
      break;
    case TimeSource::CACHED:
      label = PSTR("CACHED");
      break;
    case TimeSource::NONE:
    default:
      break;
  }
  strncpy_P(out, label, out_len - 1);
  out[out_len - 1] = '\0';
}

time_t NtpClient::getCurrentTime() const noexcept {
  return time(nullptr);
}

void NtpClient::startSync() {
  LOG_INFO("NTP", F("Syncing..."));
  // configTime automatically sends NTP packets.
  // Use pool.ntp.org with google as backup.
  configTime(AppConstants::TIMEZONE_OFFSET_SEC, 0, "pool.ntp.org", "time.google.com");

  m_sync_in_progress = true;
  m_syncTimeoutTimer.reset();
}

void NtpClient::checkSyncStatus() {
  time_t now = time(nullptr);

  // Check for valid time (> Jan 1 2024).
  if (now > NTP_VALID_TIMESTAMP_THRESHOLD) {
    m_lastSuccessMillis = millis();
    m_isSynced = true;
    m_ntpVerified = true;
    m_timeSource = TimeSource::NTP;
    m_sync_in_progress = false;
    CryptoUtils::setReplaySkewWindow(AppConstants::WS_REPLAY_SKEW_SEC_STRICT);

    // Set long retry interval (1 hour) for subsequent syncs.
    m_retryTimer.setInterval(3600UL * 1000UL);

    saveTimeSnapshot(now);
    LOG_INFO("NTP", F("Sync OK."));
  } else if (m_syncTimeoutTimer.hasElapsed(false)) {
    LOG_WARN("NTP", F("Timeout. Retrying soon."));
    m_sync_in_progress = false;
    m_retryTimer.setInterval(5000);  // Short retry (5s).
  }
}

void NtpClient::setManualTime(time_t epoch) {
  // Only accept if valid (> Jan 1 2024)
  if (epoch > NTP_VALID_TIMESTAMP_THRESHOLD) {
    struct timeval tv = {epoch, 0};
    settimeofday(&tv, nullptr);

    m_isSynced = true;
    m_lastSuccessMillis = millis();
    m_sync_in_progress = false;  // Stop any pending NTP check
    m_ntpVerified = false;       // Still try to sync NTP in background
    m_timeSource = TimeSource::MANUAL;
    CryptoUtils::setReplaySkewWindow(AppConstants::WS_REPLAY_SKEW_SEC_SOFT);
    m_retryTimer.setInterval(AppConstants::NTP_INITIAL_DELAY_MS);
    m_retryTimer.reset();

    // Defer next NTP retry for a while (e.g., 1 hour) since we have fresh time.
    // NOTE: keep retry active to allow NTP to verify later.

    saveTimeSnapshot(epoch);
    LOG_INFO("NTP", F("Manual time set: %ld"), (long)epoch);
  }
}

void NtpClient::loadSavedTime() {
  if (!LittleFS.exists(Paths::TIME_SNAPSHOT)) {
    return;
  }
  File f = LittleFS.open(Paths::TIME_SNAPSHOT, "r");
  if (!f) {
    return;
  }
  char buf[32] = {0};
  size_t n = f.readBytesUntil('\n', buf, sizeof(buf) - 1);
  f.close();
  if (n == 0) {
    return;
  }
  char* endptr = nullptr;
  unsigned long saved = strtoul(buf, &endptr, 10);
  if (saved <= NTP_VALID_TIMESTAMP_THRESHOLD) {
    return;
  }

  time_t approx = static_cast<time_t>(saved + (millis() / 1000));
  struct timeval tv = {approx, 0};
  settimeofday(&tv, nullptr);

  m_isSynced = true;      // Time is valid (approx)
  m_ntpVerified = false;  // Still need real NTP sync
  m_timeSource = TimeSource::CACHED;
  m_lastSuccessMillis = millis();
  CryptoUtils::setReplaySkewWindow(AppConstants::WS_REPLAY_SKEW_SEC_SOFT);
  LOG_WARN("NTP", F("Loaded cached time snapshot; NTP sync pending"));
}

void NtpClient::saveTimeSnapshot(time_t epoch) {
  if (epoch <= NTP_VALID_TIMESTAMP_THRESHOLD) {
    return;
  }
  File f = LittleFS.open(Paths::TIME_SNAPSHOT, "w");
  if (!f) {
    LOG_WARN("NTP", F("Failed to save time snapshot"));
    return;
  }
  char buf[24];
  int n = snprintf_P(buf, sizeof(buf), PSTR("%lu\n"), static_cast<unsigned long>(epoch));
  if (n > 0) {
    f.write(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(n));
  }
  f.close();
  m_lastSavedMillis = millis();
}
