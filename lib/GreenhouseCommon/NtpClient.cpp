#include "NtpClient.h"

#include <Arduino.h>
#include <sys/time.h>  // Required for settimeofday
#include <time.h>

#include "WifiManager.h"
#include "Logger.h"

NtpClient::NtpClient(WifiManager& wifiManager) : m_wifiManager(wifiManager) {}

void NtpClient::init() {
  m_retryTimer.setInterval(AppConstants::NTP_INITIAL_DELAY_MS);
  m_syncTimeoutTimer.setInterval(AppConstants::NTP_SYNC_TIMEOUT_MS);
}

void NtpClient::onWifiStateChanged(WifiManager::State newState) {
  if (newState == WifiManager::State::CONNECTED_STA) {
    LOG_INFO("NTP", F("WiFi connected. Triggering immediate time sync..."));
    m_isSynced = false;  // Reset flag biar maksa sync
    m_sync_in_progress = false;
    m_retryTimer.reset();  // Reset timer agar hasElapsed() true di loop berikutnya
    startSync();           // Atau langsung panggil startSync di sini
  }
}

void NtpClient::handle() {
  // Hanya jalan kalau WiFi connect
  if (m_wifiManager.getState() != WifiManager::State::CONNECTED_STA) {
    return;
  }

  // Jika belum sync dan tidak sedang loading, cek timer
  if (!m_isSynced && !m_sync_in_progress && m_retryTimer.hasElapsed()) {
    startSync();
  }

  // Cek timeout
  if (m_sync_in_progress) {
    checkSyncStatus();
  }
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
  // Config time akan otomatis mengirim packet NTP
  // Gunakan pool.ntp.org dan google sebagai backup
  configTime(AppConstants::TIMEZONE_OFFSET_SEC, 0, "pool.ntp.org", "time.google.com");

  m_sync_in_progress = true;
  m_syncTimeoutTimer.reset();
}

void NtpClient::checkSyncStatus() {
  time_t now = time(nullptr);

  // Cek apakah waktu valid (> 2024)
  if (now > 1704067200UL) {
    m_lastSuccessMillis = millis();
    m_isSynced = true;
    m_sync_in_progress = false;

    // Set timer retry panjang (misal 1 jam) buat re-sync nanti
    m_retryTimer.setInterval(3600 * 1000);

    m_retryTimer.setInterval(3600 * 1000);

    LOG_INFO("NTP", F("Sync OK."));
  } else if (m_syncTimeoutTimer.hasElapsed(false)) {
    LOG_WARN("NTP", F("Timeout. Retrying soon."));
    m_sync_in_progress = false;
    m_retryTimer.setInterval(5000);  // Retry cepat 5 detik lagi
  }
}

void NtpClient::setManualTime(time_t epoch) {
  // Only accept if valid (> 2024)
  if (epoch > 1704067200UL) {
    struct timeval tv = {epoch, 0};
    settimeofday(&tv, nullptr);

    m_isSynced = true;
    m_lastSuccessMillis = millis();
    m_sync_in_progress = false;  // Stop any pending NTP check

    // Defer next NTP retry for a while (e.g., 1 hour) since we just got fresh time
    m_retryTimer.setInterval(3600 * 1000);

    m_retryTimer.setInterval(3600 * 1000);

    LOG_INFO("NTP", F("Updated via HTTP Header fallback: %ld"), (long)epoch);
  }
}