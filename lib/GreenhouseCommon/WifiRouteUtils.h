#ifndef WIFI_ROUTE_UTILS_H
#define WIFI_ROUTE_UTILS_H

#include <span>
#include <string_view>

#include "utils.h"

class AsyncResponseStream;

namespace WifiRouteUtils {

  inline int computeSignalBars(int32_t rssi) {
    if (rssi > -50)
      return 4;
    if (rssi > -60)
      return 3;
    if (rssi > -70)
      return 2;
    if (rssi > -80)
      return 1;
    return 0;
  }

  inline void appendNetworkJsonEscaped(AsyncResponseStream& response,
                                       bool& first,
                                       const char* safeSsid,
                                       int32_t rssi,
                                       bool isOpen,
                                       bool isKnown) {
    if (!first)
      response.print(",");
    first = false;

    int bars = computeSignalBars(rssi);
    // Avoid printf-style formatting to reduce code size and heap churn.
    response.print(F("{\"ssid\":REDACTED
    response.print(safeSsid);
    response.print(F("\",\"rssi\":"));
    response.print(rssi);
    response.print(F(",\"bars\":"));
    response.print(bars);
    response.print(F(",\"open\":"));
    response.print(isOpen ? F("true") : F("false"));
    response.print(F(",\"known\":"));
    response.print(isKnown ? F("true") : F("false"));
    response.print(F("}"));
  }

  inline bool appendNetworkJson(AsyncResponseStream& response,
                                bool& first,
                                std::string_view ssid,
                                int32_t rssi,
                                bool isOpen,
                                bool isKnown) {
    char safeSsid[68];
    size_t escaped = Utils::escape_json_string(std::span{safeSsid}, ssid);
    if (escaped == 0 && !ssid.empty())
      return false;

    appendNetworkJsonEscaped(response, first, safeSsid, rssi, isOpen, isKnown);
    return true;
  }

}  // namespace WifiRouteUtils

#endif  // WIFI_ROUTE_UTILS_H
