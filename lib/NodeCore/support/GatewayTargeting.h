#ifndef GATEWAY_TARGETING_H
#define GATEWAY_TARGETING_H

#include <Arduino.h>

#include "generated/node_config.h"

namespace GatewayTargeting {

  struct GatewayPairIds {
    uint8_t primary = 1;
    uint8_t secondary = 2;
  };

  inline bool prefersGatewayGh1() {
    if (GH_ID == 1) {
      return NODE_ID <= 5;
    }
    if (GH_ID == 2) {
      return false;
    }
    return NODE_ID <= 5;
  }

  inline GatewayPairIds resolvePreferredPair() {
    const bool preferGh1 = prefersGatewayGh1();
    GatewayPairIds ids;
    ids.primary = preferGh1 ? 1 : 2;
    ids.secondary = preferGh1 ? 2 : 1;
    return ids;
  }

}  // namespace GatewayTargeting

#endif  // GATEWAY_TARGETING_H
