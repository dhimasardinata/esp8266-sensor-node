#ifndef DIAGNOSTICS_TERMINAL_H
#define DIAGNOSTICS_TERMINAL_H

#include <ESPAsyncWebServer.h>

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include "CryptoUtils.h"
#include "IAuthManager.h"
#include "constants.h"

// Forward declarations
class ICommand;
struct ServiceContainer;

// Vector ensures contiguous memory and uses less heap than map for small N
using CommandList = std::vector<std::unique_ptr<ICommand>>;

class DiagnosticsTerminal : public IAuthManager {
public:
  DiagnosticsTerminal(AsyncWebSocket& ws, ServiceContainer& services);

  DiagnosticsTerminal(const DiagnosticsTerminal&) = delete;
  DiagnosticsTerminal& operator=(const DiagnosticsTerminal&) = delete;
  ~DiagnosticsTerminal();

  void init();
  
  // IAuthManager implementation
  bool isClientAuthenticated(uint32_t clientId) const override;
  void setClientAuthenticated(uint32_t clientId, bool authenticated) override;
  bool isClientLockedOut(uint32_t clientId) const override;
  void recordFailedLogin(uint32_t clientId) override;
  void clearFailedLogins(uint32_t clientId) override;

private:
  void onEvent(AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);

  void registerCommands();
  void handleCommand(char* cmd, AsyncWebSocketClient* client);

  [[nodiscard]] bool isAuthenticated(AsyncWebSocketClient* client) const;

  void ws_println_client(AsyncWebSocketClient* client, const String& msg);

  CommandList m_commands;

  AsyncWebSocket& m_ws;
  ServiceContainer& m_services;

  // MEMORY OPTIMIZATION: Consolidated client state into fixed-size array
  // Replaces 5 separate std::maps, saving ~200 bytes per client in heap overhead
  struct ClientState {
    AsyncWebSocketClient* client = nullptr;
    unsigned long lastActivity = 0;
    unsigned long lastFailMs = 0;
    unsigned long rateWindowStart = 0;
    uint8_t failedAttempts = 0;
    uint8_t rateCount = 0;
    bool isAuthenticated = false;
    bool inUse = false;
  };
  std::array<ClientState, AppConstants::MAX_WS_CLIENTS> m_clientStates{};

  CryptoUtils::AES_CBC_Cipher m_cipher;
  
  ClientState* findClientState(uint32_t clientId);
  const ClientState* findClientState(uint32_t clientId) const;
  ClientState* allocateClientState(AsyncWebSocketClient* client);
  void freeClientState(uint32_t clientId);
  
  bool checkRateLimit(AsyncWebSocketClient* client);
  void updateClientActivity(uint32_t clientId);
  void checkSessionTimeouts();
};

#endif  // DIAGNOSTICS_TERMINAL_H