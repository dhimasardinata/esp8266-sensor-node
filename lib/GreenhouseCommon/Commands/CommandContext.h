#ifndef COMMAND_CONTEXT_H
#define COMMAND_CONTEXT_H

#include <ESPAsyncWebServer.h>

// Struct to encapsulate all command arguments
struct CommandContext {
  const char* args;

  AsyncWebSocketClient* client;
  bool isAuthenticated;
};

#endif  // COMMAND_CONTEXT_H
