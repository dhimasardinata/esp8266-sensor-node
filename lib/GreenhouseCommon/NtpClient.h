#ifndef NTP_CLIENT_H
#define NTP_CLIENT_H

#include <IntervalTimer.h>

#include "constants.h"
#include "REDACTED"  // Tambahkan ini

// Forward declare
class WifiManager;

// (Constants moved to constants.h)

class NtpClient : public IWifiStateObserver {  // Inherit Observer
public:
  enum class TimeSource : uint8_t { NONE, CACHED, MANUAL, NTP };

  explicit NtpClient(WifiManager& wifiManager);

  void init();
  void handle();

  // IWifiStateObserver implementation.
  void onWifiStateChanged(WifiManager:REDACTED

  [[nodiscard]] bool isTimeSynced() const noexcept;
  [[nodiscard]] unsigned long getLastSyncMillis() const noexcept;
  [[nodiscard]] TimeSource getTimeSource() const noexcept;
  [[nodiscard]] const char* getTimeSourceLabel() const noexcept;

  // Get current timestamp (synced or estimate based on millis drift).
  [[nodiscard]] time_t getCurrentTime() const noexcept;

  // Manual Time Setter (HTTP Fallback).
  void setManualTime(time_t epoch);

private:
  void startSync();
  void checkSyncStatus();
  void loadSavedTime();
  void saveTimeSnapshot(time_t epoch);

  IntervalTimer m_retryTimer;
  IntervalTimer m_syncTimeoutTimer;

  bool m_isSynced = false;      // Time is valid (NTP/manual/cached)
  bool m_ntpVerified = false;   // True only after real NTP sync
  TimeSource m_timeSource = TimeSource::NONE;
  bool m_sync_in_progress = false;
  unsigned long m_lastSuccessMillis = 0;
  unsigned long m_lastSavedMillis = 0;
  WifiManager& m_wifiManager;
};

#endif
