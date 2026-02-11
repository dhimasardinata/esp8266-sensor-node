#include "NtpClient.h"

#include <Arduino.h>
#include <sys/time.h>  // Required for settimeofday
#include <time.h>

#include "REDACTED"
#include "ConfigManager.h"  // For NTP_VALID_TIMESTAMP_THRESHOLD
#include "Logger.h"

NtpClient::NtpClient(WifiManager& wifiManager) : m_wifiManager(wifiManager) {}

void NtpClient::init() {
  m_retryTimer.setInterval(AppConstants::NTP_INITIAL_DELAY_MS);
  m_syncTimeoutTimer.setInterval(AppConstants::NTP_SYNC_TIMEOUT_MS);
}

void NtpClient::onWifiStateChanged(WifiManager::State newState) {
  if (newState == WifiManager::State::CONNECTED_STA) {
    LOG_INFO("REDACTED", F("REDACTED"));
    m_isSynced = false;    // Force sync requirement.
    m_sync_in_progress = false;
    m_retryTimer.reset();  // Ensure timer elapses in next loop.
    startSync();
  }
}

void NtpClient::handle() {
  if (m_wifiManager.getState() != REDACTED

  if (!m_isSynced && !m_sync_in_progress && m_retryTimer.hasElapsed()) startSync();
  if (m_sync_in_progress) checkSyncStatus();
}

bool NtpClient::isTimeSynced() const noexcept {
  return m_isSynced;
}

unsigned long NtpClient::getLastSyncMillis() const noexcept {
  return m_lastSuccessMillis;
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
    m_sync_in_progress = false;

    // Set long retry interval (1 hour) for subsequent syncs.
    m_retryTimer.setInterval(3600UL * 1000UL);

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

    // Defer next NTP retry for a while (e.g., 1 hour) since we have fresh time.
    m_retryTimer.setInterval(3600UL * 1000UL);

    LOG_INFO("NTP", F("Updated via HTTP Header fallback: %ld"), (long)epoch);
  }
}