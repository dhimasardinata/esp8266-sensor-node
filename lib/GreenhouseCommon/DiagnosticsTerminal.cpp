#include "DiagnosticsTerminal.h"

#include <algorithm>
#include <string>
#include <string_view>

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
      m_services(services),
      m_cipher(std::string_view(reinterpret_cast<const char*>(CryptoUtils::AES_KEY), 32)) {}

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
  registerCmd(std::make_unique<HelpCommand>(m_commands));
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

  // Search command using std::find_if
  auto it = std::find_if(
      m_commands.begin(), m_commands.end(), [cmd](const auto& c) { return strcmp(c->getName(), cmd) == 0; });

  if (it != m_commands.end()) {
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

void DiagnosticsTerminal::onEvent(
    AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT: {
      LOG_INFO("WS", F("Client #%u connected from %s"), client->id(), client->remoteIP().toString().c_str());

      // Try to allocate client state
      if (!allocateClientState(client)) {
        LOG_WARN("WS", F("Max clients reached, rejecting."));
        client->close();
        return;
      }

      // --- MANUAL JSON CONSTRUCTION ---
      char payload[128];
      snprintf(payload,
               sizeof(payload),
               "{\"type\":\"init\",\"nodeId\":\"%d-%d\",\"firmwareVersion\":\"%s\"}",
               GH_ID,
               NODE_ID,
               FIRMWARE_VERSION);

      // Send encrypted
      Utils::ws_send_encrypted(client, payload);
      break;
    }
    case WS_EVT_DISCONNECT: {
      LOG_INFO("WS", F("Client #%u disconnected"), client->id());
      freeClientState(client->id());
      break;
    }
    case WS_EVT_DATA: {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;

      // Check OpCode
      if (info->opcode != WS_TEXT) {
        LOG_WARN("WS-FAIL", F("Ignored: Not a TEXT frame."));
        return;
      }

      // Check Fragmentation
      if (info->index != 0 || info->len != len || !info->final) {
        LOG_WARN("WS-FAIL", F("Ignored: Fragmented packet. (NOTE: Large packets not supported on embedded)"));
        return;
      }

      // --- Size Limit Protection (SECURITY: Use constant, not magic number) ---
      if (len > AppConstants::MAX_WS_PACKET_SIZE || info->len > AppConstants::MAX_WS_PACKET_SIZE) {
        LOG_WARN("WS", F("Packet too big (%u bytes)"), (unsigned int)len);
        return;
      }

      // --- Rate Limiting ---
      if (!checkRateLimit(client)) {
        LOG_WARN("WS", F("Client #%u Rate Limit Exceeded. Ignoring."), client->id());
        return;
      }

      // 1. Convert Payload to String safely (Stack Buffer)
      char raw_payload[AppConstants::MAX_WS_PACKET_SIZE + 1];
      memcpy(raw_payload, data, len);
      raw_payload[len] = '\0';

      // Use string_view for Zero-Copy deserialization
      std::string_view serialized_view(raw_payload, len);

      // 2. Deserialize (Base64 -> IV:Cipher)
      auto payload = CryptoUtils::deserialize_payload(serialized_view);
      if (!payload) {
        LOG_WARN("WS-FAIL", F("Deserialize Failed! Invalid Base64 or format."));
        return;
      }

      // 3. Decrypt
      auto decrypted_data = m_cipher.decrypt(*payload);
      if (!decrypted_data) {
        LOG_WARN("WS-FAIL", F("Decrypt Failed! Invalid Key or tampered data."));
        return;
      }

      // 4. Parse Command
      decrypted_data->push_back('\0');
      char* cmd_buf = (char*)decrypted_data->data();

      LOG_INFO("WS", F("Decrypted Command: '%s'"), cmd_buf);

      // Skip leading whitespace
      char* p = cmd_buf;
      while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        p++;
      }

      if (*p != '\0') {
        handleCommand(p, client);
      } else {
        LOG_INFO("WS", F("Command is empty/whitespace only."));
      }

      // Update activity after processing
      updateClientActivity(client->id());
      break;
    }
    case WS_EVT_ERROR:
      LOG_ERROR("WS", F("Error: %u"), *((uint16_t*)arg));
      break;
    default:
      break;
  }
}

[[nodiscard]] bool DiagnosticsTerminal::isAuthenticated(AsyncWebSocketClient* client) const {
  if (!client)
    return false;
  return isClientAuthenticated(client->id());
}

void DiagnosticsTerminal::ws_println_client(AsyncWebSocketClient* client, const String& msg) {
  Utils::ws_printf(client, "%s\n", msg.c_str());
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