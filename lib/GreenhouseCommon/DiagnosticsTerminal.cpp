#include "DiagnosticsTerminal.h"
#include <ESPAsyncWebServer.h>

#include <algorithm>
#include <string>
#include <string_view>

#include "CompileTimeUtils.h"

#include "ApiClient.h"
#include "Commands/CacheStatusCommand.h"
#include "Commands/CheckUpdateCommand.h"
#include "Commands/ClearCacheCommand.h"
#include "Commands/CommandContext.h"
#include "Commands/CrashLogCommand.h"
#include "Commands/FactoryResetCommand.h"
#include "Commands/FormatFsCommand.h"
#include "Commands/FsStatusCommand.h"
#include "Commands/GetCalibrationCommand.h"
#include "Commands/GetConfigCommand.h"
#include "Commands/HelpCommand.h"
#include "Commands/ICommand.h"
#include "Commands/LoginCommand.h"
#include "Commands/LogoutCommand.h"
#include "Commands/OpenWifiCommand.h"
#include "Commands/QosCommand.h"
#include "Commands/ReadSensorsCommand.h"
#include "Commands/RebootCommand.h"
#include "Commands/ResetCalibrationCommand.h"
#include "Commands/SendNowCommand.h"
#include "Commands/SetCalibrationCommand.h"
#include "Commands/SetConfigCommand.h"
#include "Commands/SetPortalPassCommand.h"
#include "Commands/SetTokenCommand.h"
#include "Commands/SetWifiCommand.h"
#include "Commands/StatusCommand.h"
#include "Commands/SysInfoCommand.h"
#include "Commands/WifiAddCommand.h"
#include "Commands/WifiListCommand.h"
#include "Commands/WifiRemoveCommand.h"
#include "Commands/ZeroCalibrationCommand.h"
#include "Commands/ModeCommand.h"
#include "ConfigManager.h"
#include "CryptoUtils.h"
#include "ICacheManager.h"
#include "NtpClient.h"
#include "OtaManager.h"
#include "SensorManager.h"
#include "ServiceContainer.h"
#include "WifiManager.h"
#include "node_config.h"
#include "utils.h"
#include "Logger.h"

DiagnosticsTerminal::DiagnosticsTerminal(AsyncWebSocket& ws, ServiceContainer& services)
    : m_ws(ws),
      m_services(services) {}

DiagnosticsTerminal::~DiagnosticsTerminal() = default;

void DiagnosticsTerminal::init() {
  registerCommands();
  m_ws.onEvent([this](AsyncWebSocket* server,
                      AsyncWebSocketClient* client,
                      AwsEventType type,
                      void* arg,
                      uint8_t* data,
                      size_t len) { this->onEvent(client, type, arg, data, len); });
}

// ============================================================================
// ClientState helper methods (MEMORY OPTIMIZATION)
// ============================================================================

DiagnosticsTerminal::ClientState* DiagnosticsTerminal::findClientState(uint32_t clientId) {
  for (auto& state : m_clientStates) {
    if (state.inUse && state.client && state.client->id() == clientId) {
      return &state;
    }
  }
  return nullptr;
}

const DiagnosticsTerminal::ClientState* DiagnosticsTerminal::findClientState(uint32_t clientId) const {
  for (const auto& state : m_clientStates) {
    if (state.inUse && state.client && state.client->id() == clientId) {
      return &state;
    }
  }
  return nullptr;
}

DiagnosticsTerminal::ClientState* DiagnosticsTerminal::allocateClientState(AsyncWebSocketClient* client) {
  for (auto& state : m_clientStates) {
    if (!state.inUse) {
      state = ClientState{};  // Reset all fields
      state.client = client;
      state.inUse = true;
      state.lastActivity = millis();
      return &state;
    }
  }
  return nullptr;  // No free slots
}

void DiagnosticsTerminal::freeClientState(uint32_t clientId) {
  if (auto* state = findClientState(clientId)) {
    *state = ClientState{};  // Reset to defaults
  }
}

// ============================================================================
// IAuthManager implementation
// ============================================================================

bool DiagnosticsTerminal::isClientAuthenticated(uint32_t clientId) const {
  if (const auto* state = findClientState(clientId)) {
    return state->isAuthenticated;
  }
  return false;
}

void DiagnosticsTerminal::setClientAuthenticated(uint32_t clientId, bool authenticated) {
  if (auto* state = findClientState(clientId)) {
    state->isAuthenticated = authenticated;
  }
}

bool DiagnosticsTerminal::isClientLockedOut(uint32_t clientId) const {
  if (const auto* state = findClientState(clientId)) {
    if (state->failedAttempts >= AppConstants::MAX_FAILED_AUTH_ATTEMPTS) {
      if (millis() - state->lastFailMs < AppConstants::AUTH_LOCKOUT_DURATION_MS) {
        return true;
      }
    }
  }
  return false;
}

void DiagnosticsTerminal::recordFailedLogin(uint32_t clientId) {
  if (auto* state = findClientState(clientId)) {
    state->failedAttempts++;
    state->lastFailMs = millis();
  }
}

void DiagnosticsTerminal::clearFailedLogins(uint32_t clientId) {
  if (auto* state = findClientState(clientId)) {
    state->failedAttempts = 0;
    state->lastFailMs = 0;
  }
}

// ============================================================================
// Command registration
// ============================================================================

void DiagnosticsTerminal::registerCommands() {
  // OPTIMIZATION: Pre-allocate vector to avoid reallocations
  m_commands.reserve(30);

  auto registerCmd = [&](std::unique_ptr<ICommand> cmd) { m_commands.push_back(std::move(cmd)); };
  registerCmd(std::make_unique<CacheStatusCommand>(m_services.cacheManager));
  registerCmd(
      std::make_unique<CheckUpdateCommand>(m_services.otaManager, m_services.wifiManager, m_services.ntpClient));
  registerCmd(std::make_unique<ClearCacheCommand>(m_services.cacheManager));
  registerCmd(std::make_unique<ClearCrashCommand>());
  registerCmd(std::make_unique<CrashLogCommand>());
  registerCmd(std::make_unique<FactoryResetCommand>(m_services.configManager, m_services.cacheManager));
  registerCmd(std::make_unique<FormatFsCommand>(m_services.configManager));
  registerCmd(std::make_unique<FsStatusCommand>());
  registerCmd(std::make_unique<GetCalibrationCommand>(m_services.configManager));
  registerCmd(std::make_unique<GetConfigCommand>(m_services.configManager));
  registerCmd(std::make_unique<LoginCommand>(m_services.configManager, *this));
  registerCmd(std::make_unique<LogoutCommand>(*this));
  registerCmd(std::make_unique<QosUploadCommand>(m_services.apiClient));
  registerCmd(std::make_unique<QosOtaCommand>(m_services.apiClient));
  registerCmd(std::make_unique<OpenWifiCommand>(m_services.wifiManager));
  registerCmd(std::make_unique<ReadSensorsCommand>(m_services.sensorManager, m_services.configManager));
  registerCmd(std::make_unique<RebootCommand>());
  registerCmd(std::make_unique<ResetCalibrationCommand>(m_services.configManager));
  registerCmd(std::make_unique<SendNowCommand>(m_services.apiClient));
  registerCmd(std::make_unique<SetCalibrationCommand>(m_services.configManager));
  registerCmd(std::make_unique<SetConfigCommand>(m_services.configManager));
  registerCmd(std::make_unique<SetPortalPassCommand>(m_services.configManager));
  registerCmd(std::make_unique<SetTokenCommand>(m_services.configManager));
  registerCmd(std::make_unique<SetWifiCommand>(m_services.configManager));
  registerCmd(std::make_unique<StatusCommand>(
      m_services.wifiManager, m_services.ntpClient, m_services.apiClient, m_services.sensorManager));
  registerCmd(std::make_unique<SysInfoCommand>());
  registerCmd(std::make_unique<WifiListCommand>(m_services.wifiManager));
  registerCmd(std::make_unique<WifiAddCommand>(m_services.wifiManager));
  registerCmd(std::make_unique<WifiRemoveCommand>(m_services.wifiManager));
  registerCmd(std::make_unique<ZeroCalibrationCommand>(m_services.configManager));
  registerCmd(std::make_unique<ModeCommand>(m_services.apiClient));
  registerCmd(std::make_unique<HelpCommand>(m_commands));

  // OPTIMIZATION: Sort by hash for O(log N) binary search lookup
  std::sort(m_commands.begin(), m_commands.end(),
            [](const std::unique_ptr<ICommand>& a, const std::unique_ptr<ICommand>& b) {
              return a->getNameHash() < b->getNameHash();
            });
}

void DiagnosticsTerminal::handleCommand(char* cmd, AsyncWebSocketClient* client) {
  // Echo back the command
  Utils::ws_printf(client, "> %s\n", cmd);

  // Split Command and Args in-place
  char* args = strchr(cmd, ' ');
  if (args) {
    *args = '\0';  // Null terminate command name
    args++;        // Point to start of arguments

    // Trim leading spaces from args
    while (*args == ' ')
      args++;
  } else {
    args = const_cast<char*>("");
  }

  // OPTIMIZATION: Use Binary Search (O(log N)) instead of Linear Scan (O(N))
  uint32_t cmdHash = CompileTimeUtils::rt_hash(cmd);

  auto it = std::lower_bound(m_commands.begin(), m_commands.end(), cmdHash,
                             [](const std::unique_ptr<ICommand>& cmd, uint32_t hash) {
                               return cmd->getNameHash() < hash;
                             });

  if (it != m_commands.end() && (*it)->getNameHash() == cmdHash) {
    ICommand* command = it->get();

    // HARDENING: Null pointer guard
    if (!command) {
      LOG_ERROR("DIAG", F("Command pointer is null"));
      return;
    }

    bool isAuth = isAuthenticated(client);

    // Create context (args, client, isAuthenticated)
    CommandContext context{args, client, isAuth};

    // Check Auth
    if (command->requiresAuth() && !isAuth) {
      ws_println_client(client, F("[ERROR] Access Denied. Please 'login <password>' first."));
      return;
    }

    command->execute(context);

  } else {
    Utils::ws_printf(client, "[ERROR] Unknown command: '%s'. Type 'help'.\n", cmd);
  }
}

void DiagnosticsTerminal::handleConnect(AsyncWebSocketClient* client) {
  LOG_INFO("WS", F("Client #%u connected from %s"), client->id(), client->remoteIP().toString().c_str());

  if (!allocateClientState(client)) {
    LOG_WARN("WS", F("Max clients reached, rejecting."));
    client->close();
    return;
  }

  char payload[128];
  snprintf(payload, sizeof(payload),
           "{\"type\":\"init\",\"nodeId\":\"%d-%d\",\"firmwareVersion\":\"%s\"}",
           GH_ID, NODE_ID, FIRMWARE_VERSION);
  Utils::ws_send_encrypted(client, payload);
}

bool DiagnosticsTerminal::isValidFrame(AwsFrameInfo* info, size_t len, AsyncWebSocketClient* client) {
  if (info->opcode != WS_TEXT) { LOG_DEBUG("WS", F("Not TEXT")); return false; }
  if (info->index != 0 || info->len != len || !info->final) { LOG_DEBUG("WS", F("Fragmented")); return false; }
  if (!checkRateLimit(client)) { LOG_DEBUG("WS", F("Rate limit")); return false; }
  return true;
}

void DiagnosticsTerminal::handleDataFrame(AsyncWebSocketClient* client, void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;

  // Early validation with debug logging
  if (!isValidFrame(info, len, client)) return;
  
  // Buffer size validation (CRITICAL: must check before memcpy)
  constexpr size_t MAX_LEN = AppConstants::MAX_WS_PACKET_SIZE;
  if (len == 0 || len > MAX_LEN) { LOG_WARN("WS", F("Bad size: %zu"), len); return; }

  // Safe copy with validated length
  // Safe copy with validated length
  char raw_payload[MAX_LEN + 1];
  // Explicitly limit copy to buffer size for static analysis safety
  size_t copy_len = std::min(len, sizeof(raw_payload) - 1);
  memcpy(raw_payload, data, copy_len);
  raw_payload[copy_len] = '\0';

  // Decrypt payload
  auto payload = CryptoUtils::deserialize_payload(std::string_view(raw_payload, len));
  if (!payload) { LOG_WARN("WS", F("Deserialize failed")); return; }

  auto decrypted = CryptoUtils::getSharedCipher().decrypt(*payload);
  if (!decrypted) { LOG_WARN("WS", F("Decrypt failed")); return; }

  // Process command
  decrypted->push_back('\0');
  char* cmd = (char*)decrypted->data();
  while (*cmd && (*cmd == ' ' || *cmd == '\t' || *cmd == '\r' || *cmd == '\n')) cmd++;

  if (*cmd != '\0') handleCommand(cmd, client);
  updateClientActivity(client->id());
}

void DiagnosticsTerminal::onEvent(
    AsyncWebSocketClient* client, int type, void* arg, uint8_t* data, size_t len) {
  switch ((AwsEventType)type) {
    case WS_EVT_CONNECT:    handleConnect(client); break;
    case WS_EVT_DISCONNECT: LOG_INFO("WS", F("Client #%u disconnected"), client->id()); freeClientState(client->id()); break;
    case WS_EVT_DATA:       handleDataFrame(client, arg, data, len); break;
    case WS_EVT_ERROR:      LOG_ERROR("WS", F("Error: %u"), *((uint16_t*)arg)); break;
    default: break;
  }
}

[[nodiscard]] bool DiagnosticsTerminal::isAuthenticated(AsyncWebSocketClient* client) const {
  if (!client)
    return false;
  return isClientAuthenticated(client->id());
}

void DiagnosticsTerminal::ws_println_client(AsyncWebSocketClient* client, const char* msg) {
  Utils::ws_printf(client, "%s\n", msg);
}

void DiagnosticsTerminal::ws_println_client(AsyncWebSocketClient* client, const __FlashStringHelper* msg) {
  // Fallback to String for Flash strings to ensure safe copying to RAM before printing
  // This keeps the efficient const char* path for RAM strings while handling F() correctly
  Utils::ws_printf(client, "%s\n", String(msg).c_str());
}

bool DiagnosticsTerminal::checkRateLimit(AsyncWebSocketClient* client) {
  if (!client)
    return false;

  auto* state = findClientState(client->id());
  if (!state)
    return false;

  unsigned long now = millis();
  if (now - state->rateWindowStart > 1000) {
    // New Window
    state->rateWindowStart = now;
    state->rateCount = 1;
    return true;
  }

  // Within Window
  state->rateCount++;
  if (state->rateCount > 5) {
    // Limit Exceeded (5 commands per sec)
    return false;
  }
  return true;
}

// HARDENING: Session activity tracking
void DiagnosticsTerminal::updateClientActivity(uint32_t clientId) {
  if (auto* state = findClientState(clientId)) {
    state->lastActivity = millis();
  }
}

// HARDENING: Check and disconnect inactive clients
void DiagnosticsTerminal::checkSessionTimeouts() {
  unsigned long now = millis();

  for (auto& state : m_clientStates) {
    if (state.inUse && state.client) {
      if (now - state.lastActivity > AppConstants::WS_SESSION_TIMEOUT_MS) {
        LOG_WARN("SECURITY", F("Client #%u session timed out (30min inactivity)"), state.client->id());
        state.client->close();
        // Note: freeClientState will be called in WS_EVT_DISCONNECT handler
      }
    }
  }
}