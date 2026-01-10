#ifndef I_WIFI_STATE_OBSERVER_H
#define I_WIFI_STATE_OBSERVER_H

#include "WifiManager.h"  // Needed for the State enum

/**
 * @brief Interface for classes that need to be notified of WiFi state changes.
 */
class IWifiStateObserver {
public:
  virtual ~IWifiStateObserver() = default;

  /**
   * @brief Called by the WifiManager whenever its state changes.
   * @param newState The new state of the WiFi connection.
   */
  virtual void onWifiStateChanged(WifiManager::State newState) = 0;
};

#endif  // I_WIFI_STATE_OBSERVER_H