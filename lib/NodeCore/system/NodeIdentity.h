#ifndef NODE_IDENTITY_H
#define NODE_IDENTITY_H

#include <Arduino.h>

#include "generated/node_config.h"

namespace NodeIdentity {

  static constexpr size_t kUserAgentBufferLen = 48;
  static constexpr size_t kDeviceIdBufferLen = 24;

  inline void buildUserAgent(char* out, size_t out_len) {
    if (!out || out_len == 0) {
      return;
    }
    const int written = snprintf_P(out,
                                   out_len,
                                   PSTR("Greenhouse-Atomic-IoT-Node-GH%u-N%u"),
                                   static_cast<unsigned>(GH_ID),
                                   static_cast<unsigned>(NODE_ID));
    if (written < 0) {
      out[0] = '\0';
      return;
    }
    out[out_len - 1] = '\0';
  }

  inline void buildDeviceId(char* out, size_t out_len) {
    if (!out || out_len == 0) {
      return;
    }
    const int written =
        snprintf_P(out, out_len, PSTR("node-gh%u-n%u"), static_cast<unsigned>(GH_ID), static_cast<unsigned>(NODE_ID));
    if (written < 0) {
      out[0] = '\0';
      return;
    }
    out[out_len - 1] = '\0';
  }

}  // namespace NodeIdentity

#endif  // NODE_IDENTITY_H
