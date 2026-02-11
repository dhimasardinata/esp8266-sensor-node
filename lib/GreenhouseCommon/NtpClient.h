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
  explicit NtpClient(WifiManager& wifiManager);

  void init();
  void handle();

  // IWifiStateObserver implementation.
  void onWifiStateChanged(WifiManager:REDACTED

  [[nodiscard]] bool isTimeSynced() const noexcept;
  [[nodiscard]] unsigned long getLastSyncMillis() const noexcept;

  // Get current timestamp (synced or estimate based on millis drift).
  [[nodiscard]] time_t getCurrentTime() const noexcept;

  // Manual Time Setter (HTTP Fallback).
  void setManualTime(time_t epoch);

private:
  void startSync();
  void checkSyncStatus();

  IntervalTimer m_retryTimer;
  IntervalTimer m_syncTimeoutTimer;

  bool m_isSynced = false;
  bool m_sync_in_progress = false;
  unsigned long m_lastSuccessMillis = 0;
  WifiManager& m_wifiManager;
};

#endif