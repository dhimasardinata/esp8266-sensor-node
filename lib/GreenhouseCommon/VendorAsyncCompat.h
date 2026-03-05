#ifndef VENDOR_ASYNC_COMPAT_H
#define VENDOR_ASYNC_COMPAT_H

#include <ESPAsyncWebServer.h>
#include <AsyncWebServerVersion.h>

#ifndef ASYNCWEBSERVER_VERSION_NUM
#error "ESPAsyncWebServer version macros are required"
#endif

static_assert(
    ASYNCWEBSERVER_VERSION_NUM >= ASYNCWEBSERVER_VERSION_VAL(3, 10, 0),
    "GreenhouseCommon requires vendored ESPAsyncWebServer >= 3.10.0");

namespace VendorAsync {

  inline bool ws_client_writable(const AsyncWebSocketClient* client) {
    return client && client->canSend() && !client->queueIsFull();
  }

  inline bool ws_all_writable(AsyncWebSocket& ws) {
    return ws.count() > 0 && ws.availableForWriteAll();
  }

}  // namespace VendorAsync

#endif
