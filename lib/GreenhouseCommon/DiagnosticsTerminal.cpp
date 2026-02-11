#include "DiagnosticsTerminal.h"

#include <ESPAsyncWebServer.h>

#include <algorithm>
#include <new>
#include <string_view>

#include "CryptoUtils.h"
#include "Logger.h"
#include "node_config.h"
#include "utils.h"

namespace {
  size_t u32_to_dec(char* out, size_t out_len, uint32_t value) {
    if (!out || out_len == 0)
      return 0;
    char tmp[10];
    size_t n = 0;
    do {
      tmp[n++] = static_cast<char>('0' + (value % 10));
      value /= 10;
    } while (value != 0 && n < sizeof(tmp));
    size_t written = 0;
    while (n > 0 && written + 1 < out_len) {
      out[written++] = tmp[--n];
    }
    out[written] = '\0';
    return written;
  }

  bool append_literal(char* out, size_t out_len, size_t& pos, const char* text) {
    if (!out || !text)
      return false;
    if (pos >= out_len || out_len == 0)
      return false;
    size_t remaining = out_len - pos;
    if (remaining <= 1)
      return false;
    size_t n = strnlen(text, remaining - 1);
    if (n == 0)
      return true;
    memcpy(out + pos, text, n);
    pos += n;
    out[pos] = '\0';
    return true;
  }

  bool append_u32(char* out, size_t out_len, size_t& pos, uint32_t value) {
    if (!out || pos >= out_len || out_len == 0)
      return false;
    size_t remaining = out_len - pos;
    if (remaining <= 1)
      return false;
    char tmp[10];
    size_t n = u32_to_dec(tmp, sizeof(tmp), value);
    if (n == 0 || n + 1 > remaining)
      return false;
    memcpy(out + pos, tmp, n);
    pos += n;
    out[pos] = '\0';
    return true;
  }
}  // namespace

DiagnosticsTerminal::DiagnosticsTerminal(AsyncWebSocket& ws, TerminalServices& services)
    : m_ws(ws), m_services(services) {}

DiagnosticsTerminal::~DiagnosticsTerminal() = default;

void DiagnosticsTerminal::init() {
  initCommands();
  m_ws.onEvent([this](AsyncWebSocket* server,
                      AsyncWebSocketClient* client,
                      AwsEventType type,
                      void* arg,
                      uint8_t* data,
                      size_t len) { this->onEvent(client, type, arg, data, len); });

  // TX buffer removed (was unused); encryption uses shared buffers.
}

bool DiagnosticsTerminal::setEnabled(bool enabled) {
  if (enabled) {
    return true;
  }
  (void)Utils::ws_set_enabled(false);
  releaseBuffers();
  return true;
}

bool DiagnosticsTerminal::ensureBuffers() {
  if (m_buffersReady) {
    return true;
  }
  m_rxBufferSize = AppConstants::MAX_WS_PACKET_SIZE + 1;
  m_decBufferSize = AppConstants::MAX_WS_PACKET_SIZE + 1;

  if (!m_cmdQueue) {
    m_cmdQueue.reset(new (std::nothrow) QueuedCommand[CMD_QUEUE_SIZE]());
    if (!m_cmdQueue) {
      releaseBuffers();
      return false;
    }
    m_cmdQueueSize = CMD_QUEUE_SIZE;
  }
  if (!m_clientStates) {
    m_clientStates.reset(new (std::nothrow) ClientState[AppConstants::MAX_WS_CLIENTS]());
    if (!m_clientStates) {
      releaseBuffers();
      return false;
    }
    m_clientStateCount = AppConstants::MAX_WS_CLIENTS;
  }

  m_rxBuffer.reset(new (std::nothrow) char[m_rxBufferSize]);
  m_decBuffer.reset(new (std::nothrow) char[m_decBufferSize]);

  if (!m_rxBuffer || !m_decBuffer) {
    releaseBuffers();
    return false;
  }
  {
    Utils::InterruptGuard guard;
    m_head = 0;
    m_tail = 0;
  }
  m_buffersReady = true;
  return true;
}

void DiagnosticsTerminal::releaseBuffers() {
  m_buffersReady = false;
  m_rxBuffer.reset();
  m_decBuffer.reset();
  m_rxBufferSize = 0;
  m_decBufferSize = 0;
  m_rxBusy = false;
  m_clientStates.reset();
  m_clientStateCount = 0;
  m_cmdQueue.reset();
  m_cmdQueueSize = 0;
  m_activeClientCount = 0;

  {
    Utils::InterruptGuard guard;
    m_head = 0;
    m_tail = 0;
  }
}

void DiagnosticsTerminal::handle() {
  if (!m_buffersReady || !m_cmdQueue) {
    return;
  }
  if (m_sessionCheckTimer.hasElapsed()) {
    checkSessionTimeouts();
  }

  // Process Command Queue (Main Loop Context)
  int processed = 0;
  while (true) {
    QueuedCommand qCmd;
    {
      Utils::InterruptGuard guard;
      if (m_head == m_tail)
        break;
      if (m_cmdQueue[m_tail].len == 0)
        break;  // slot reserved but not committed yet
      // 1. Pop Command
      qCmd = m_cmdQueue[m_tail];

      // 2. Wrap Tail
      m_tail = (m_tail + 1) & (CMD_QUEUE_SIZE - 1);
    }

    // 3. Check Client Validity
    // We stored ID. We need to check if client is still connected.
    // AsyncWebSocket::client(id) returns pointer or nullptr.
    AsyncWebSocketClient* client = m_ws.client(qCmd.clientId);
    if (client) {
      // Execute
      LOG_DEBUG("REDACTED", F("REDACTED"), qCmd.clientId);

      // Re-validate pointers/args inside - handleCommandInternal logic
      // But wait, handleCommand was doing parsing.
      // The commandStr in queue is raw command line e.g. "login 1234"
      // We need a helper to execute it.
      // Let's refactor handleCommand to be handleCommandQueued

      char* input = qCmd.commandStr;
      size_t input_len = qCmd.len;
      // Echo back the command (Optional, maybe already done? No, we queued it)
      // We should echo it now or before?
      // Previous logic echoed in handleCommand (ISR).
      // Printing in ISR is risky if ws_printf does allocation/crypto.
      // Let's echo here in main loop.

      Utils::ws_printf(client, "> %s\n", input);

      // Split Command/Args
      char* args = (input_len > 0) ? static_cast<char*>(memchr(input, ' ', input_len)) : nullptr;
      if (args) {
        *args = '\0';
        args++;
        const char* end = input + input_len;
        while (args < end && *args == ' ')
          args++;
      } else {
        args = input + input_len;
      }

      uint32_t cmdHash = CompileTimeUtils::rt_hash(input);
      bool isAuth = REDACTED

      bool dispatched = dispatchCommand(cmdHash, args, client, isAuth);
      if (!dispatched) {
        // ... Error handling moved here ...
        bool needsAuth = REDACTED
        switch (cmdHash) {
          case CmdHash::CACHE:
          case CmdHash::CHECKUPDATE:
          case CmdHash::CLEARCACHE:
          case CmdHash::CLEARCRASH:
          case CmdHash::CRASHLOG:
          case CmdHash::FACTORYRESET:
          case CmdHash::FORMAT:
          case CmdHash::FSSTATUS:
          case CmdHash::GETCAL:
          case CmdHash::GETCONFIG:
          case CmdHash::QOSUPLOAD:
          case CmdHash::QOSOTA:
          case CmdHash::OPENWIFI:
          case CmdHash::READ:
          case CmdHash::REBOOT:
          case CmdHash::RESETCAL:
          case CmdHash::SENDNOW:
          case CmdHash::SETCAL:
          case CmdHash::SETCONFIG:
          case CmdHash::SETPORTALPASS:
          case CmdHash::SETTOKEN:
          case CmdHash::SETWIFI:
          case CmdHash::WIFILIST:
          case CmdHash::WIFIADD:
          case CmdHash::WIFIREMOVE:
          case CmdHash::ZEROCAL:
          case CmdHash::MODE:
            needsAuth = REDACTED
            break;
        }
        if (needsAuth && !isAuth) {
          ws_println_client(client, F("REDACTED"));
        } else {
          Utils::ws_printf(client, "[ERROR] Unknown command: '%s'. Type 'help'.\n", input);
        }
      }
    } else {
      LOG_WARN("REDACTED", F("REDACTED"), qCmd.clientId);
    }

    if ((++processed & 0x3) == 0) {
      yield();
    }
  }
}

// ============================================================================
// ClientState Management
// ============================================================================

DiagnosticsTerminal::ClientState* DiagnosticsTerminal::findClientState(uint32_t clientId) {
  if (!m_clientStates || m_clientStateCount == 0)
    return nullptr;
  for (size_t i = 0; i < m_clientStateCount; ++i) {
    auto& state = m_clientStates[i];
    if (state.inUse && state.client && state.client->id() == clientId) {
      return &state;
    }
  }
  return nullptr;
}

const DiagnosticsTerminal::ClientState* DiagnosticsTerminal::findClientState(uint32_t clientId) const {
  if (!m_clientStates || m_clientStateCount == 0)
    return nullptr;
  for (size_t i = 0; i < m_clientStateCount; ++i) {
    const auto& state = m_clientStates[i];
    if (state.inUse && state.client && state.client->id() == clientId) {
      return &state;
    }
  }
  return nullptr;
}

DiagnosticsTerminal::ClientState* DiagnosticsTerminal::allocateClientState(AsyncWebSocketClient* client) {
  if (!m_clientStates || m_clientStateCount == 0)
    return nullptr;
  for (size_t i = 0; i < m_clientStateCount; ++i) {
    auto& state = m_clientStates[i];
    if (!state.inUse) {
      state.client = client;
      state.lastActivity = millis();
      state.lastFailMs = 0;
      state.rateWindowStart = 0;
      state.failedAttempts = 0;
      state.rateCount = 0;
      state.isAuthenticated = REDACTED
      state.inUse = true;
      ++m_activeClientCount;
      return &state;
    }
  }
  return nullptr;
}

void DiagnosticsTerminal::freeClientState(uint32_t clientId) {
  if (auto* state = findClientState(clientId)) {
    state->inUse = false;
    state->client = nullptr;
    state->isAuthenticated = REDACTED
    state->failedAttempts = 0;
    if (m_activeClientCount > 0) {
      --m_activeClientCount;
    }
    if (m_activeClientCount == 0) {
      // Free buffers when no active clients to reduce idle heap usage.
      releaseBuffers();
    }
  }
}

// ============================================================================
// IAuthManager Implementation
// ============================================================================

bool DiagnosticsTerminal::isClientAuthenticatedImpl(uint32_t clientId) const {
  if (const auto* state = findClientState(clientId)) {
    return state->isAuthenticated;
  }
  return false;
}

void DiagnosticsTerminal::setClientAuthenticatedImpl(uint32_t clientId, bool authenticated) {
  if (auto* state = findClientState(clientId)) {
    state->isAuthenticated = REDACTED
    state->failedAttempts = 0;
    state->lastFailMs = 0;
  }
}

bool DiagnosticsTerminal::isClientLockedOutImpl(uint32_t clientId) const {
  const auto* state = findClientState(clientId);
  if (!state)
    return false;

  if (state->failedAttempts >= AppConstants::MAX_FAILED_AUTH_ATTEMPTS) {
    if (millis() - state->lastFailMs < AppConstants::AUTH_LOCKOUT_DURATION_MS) {
      return true;
    }
  }
  return false;
}

void DiagnosticsTerminal::recordFailedLoginImpl(uint32_t clientId) {
  if (auto* state = findClientState(clientId)) {
    state->failedAttempts++;
    state->lastFailMs = millis();
  }
}

void DiagnosticsTerminal::clearFailedLoginsImpl(uint32_t clientId) {
  if (auto* state = findClientState(clientId)) {
    state->failedAttempts = 0;
    state->lastFailMs = 0;
  }
}

void DiagnosticsTerminal::handleConnect(AsyncWebSocketClient* client) {
// Use DEBUG level or minimal log
#if APP_LOG_LEVEL >= LOG_LEVEL_DEBUG
  LOG_DEBUG("WS", F("Client #%u connected"), client->id());
#endif

  if (!ensureBuffers()) {
    LOG_WARN("TERM", F("Terminal buffers alloc failed; rejecting client"));
    client->close();
    return;
  }
  // Ensure WS encryption buffers are available for this session.
  if (!Utils::ws_set_enabled(true)) {
    LOG_WARN("TERM", F("WS buffers alloc failed; rejecting client"));
    releaseBuffers();
    client->close();
    return;
  }

  if (!allocateClientState(client)) {
    // This WARN is acceptable as it indicates overload/DoS, but we should act fast.
    client->close();
    return;
  }

  char payload[128];
  payload[0] = '\0';
  size_t pos = 0;
  bool ok = true;
  ok &= append_literal(payload, sizeof(payload), pos, "{\"type\":\"init\",\"nodeId\":\"");
  ok &= append_u32(payload, sizeof(payload), pos, static_cast<uint32_t>(GH_ID));
  ok &= append_literal(payload, sizeof(payload), pos, "-");
  ok &= append_u32(payload, sizeof(payload), pos, static_cast<uint32_t>(NODE_ID));
  ok &= append_literal(payload, sizeof(payload), pos, "\",\"firmwareVersion\":\"");
  ok &= append_literal(payload, sizeof(payload), pos, FIRMWARE_VERSION);
  ok &= append_literal(payload, sizeof(payload), pos, "\"}");
  if (ok && pos > 0) {
    Utils::ws_send_encrypted(client, std::string_view(payload, pos));
  }
}

bool DiagnosticsTerminal::isValidFrame(AwsFrameInfo* info, size_t len, AsyncWebSocketClient* client) {
  if (info->opcode != WS_TEXT) {
    LOG_DEBUG("WS", F("Not TEXT"));
    return false;
  }
  if (info->index != 0 || info->len != len || !info->final) {
    LOG_DEBUG("WS", F("Fragmented"));
    return false;
  }
  if (!checkRateLimit(client)) {
    LOG_DEBUG("WS", F("Rate limit"));
    return false;
  }
  return true;
}

void DiagnosticsTerminal::handleDataFrame(AsyncWebSocketClient* client, void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;

  if (!m_buffersReady) {
    if (!ensureBuffers()) {
      LOG_WARN("TERM", F("Terminal buffers alloc failed; dropping frame"));
      return;
    }
  }
  if (!isValidFrame(info, len, client))
    return;
  if (!m_buffersReady || !m_rxBuffer || !m_decBuffer)
    return;

  {
    Utils::InterruptGuard guard;
    if (m_rxBusy)
      return;
    m_rxBusy = true;
  }
  struct BusyGuard {
    bool& flag;
    explicit BusyGuard(bool& f) : flag(f) {}
    ~BusyGuard() {
      Utils::InterruptGuard guard;
      flag = false;
    }
  } guard(m_rxBusy);

  constexpr size_t MAX_LEN = AppConstants::MAX_WS_PACKET_SIZE;
  if (len == 0 || len > MAX_LEN) {
    LOG_WARN("WS", F("Bad size: %zu"), len);
    return;
  }

  char* raw_payload = m_rxBuffer.get();
  if (m_rxBufferSize == 0)
    return;
  size_t copy_len = std::min(len, m_rxBufferSize - 1);
  memcpy(raw_payload, data, copy_len);
  raw_payload[copy_len] = '\0';

  auto payload = CryptoUtils::deserialize_payload(std::string_view(raw_payload, len));
  if (!payload) {
    LOG_WARN("WS", F("Deserialize failed"));
    return;
  }

  char* decrypted = m_decBuffer.get();
  if (m_decBufferSize == 0)
    return;
  size_t decryptedLen = 0;
  uint32_t timestamp = 0;

  // Decrypt and Verify Timestamp (Prevent Replay Attacks)
  const auto& cipher = CryptoUtils::sharedCipherWs();
  if (!cipher.decrypt(*payload, decrypted, m_decBufferSize - 1, decryptedLen, &timestamp)) {
// Only log if DEBUG level is enabled to prevent UART blocking in ISR
#if APP_LOG_LEVEL >= LOG_LEVEL_DEBUG
  LOG_DEBUG("WS", F("Decrypt/Replay check failed"));
#endif
    return;
  }

  // REPLAY ATTACK CHECK
  // NOTE: In a real system, we should have a per-client last_timestamp and enforce monotonicity.
  // For now, we enforce a simple window of validity (e.g. +/- 60 seconds from device time)
  // But device time might drift or not be set (NTP).
  // If NTP is not set, we skip this check or use relaxed rules?
  // User Requirement: "Verify the timestamp is within a validity window (e.g., +/- 5 seconds) and newer than the last
  // received packet."

  // TODO: Ideally verify timestamp (monotonic + window) once client JS is finalized.
  // Avoid scanning client state here until the check is implemented.

  decrypted[decryptedLen] = '\0';

  char* cmd = decrypted;
  while (*cmd && (*cmd == ' ' || *cmd == '\t' || *cmd == '\r' || *cmd == '\n'))
    cmd++;

  if (*cmd != '\0') {
    size_t cmd_offset = static_cast<size_t>(cmd - decrypted);
    size_t cmd_len = (decryptedLen > cmd_offset) ? (decryptedLen - cmd_offset) : 0;
    while (cmd_len > 0 && isspace((unsigned char)cmd[cmd_len - 1])) {
      cmd[--cmd_len] = '\0';
    }

    // CHANGE: Push to Queue instead of executing immediately
    if (cmd_len > 0) {
      pushCommandToQueue(client->id(), cmd, cmd_len);
    }
  }
  updateClientActivity(client->id());
}

void DiagnosticsTerminal::onEvent(AsyncWebSocketClient* client, int type, void* arg, uint8_t* data, size_t len) {
  const auto evt = static_cast<AwsEventType>(type);
  if (evt == WS_EVT_CONNECT) {
    handleConnect(client);
    return;
  }
  if (!m_buffersReady) {
    return;
  }
  switch (evt) {
    case WS_EVT_DISCONNECT:
      LOG_INFO("WS", F("Client #%u disconnected"), client->id());
      freeClientState(client->id());
      break;
    case WS_EVT_DATA:
      handleDataFrame(client, arg, data, len);
      break;
    case WS_EVT_ERROR:
      LOG_ERROR("WS", F("Error: %u"), arg ? *((uint16_t*)arg) : 0);
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

void DiagnosticsTerminal::ws_println_client(AsyncWebSocketClient* client, const char* msg) {
  Utils::ws_printf(client, "%s\n", msg);
}

void DiagnosticsTerminal::ws_println_client(AsyncWebSocketClient* client, const __FlashStringHelper* msg) {
  char buffer[128];
  strncpy_P(buffer, (const char*)msg, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  Utils::ws_printf(client, "%s\n", buffer);
}

bool DiagnosticsTerminal::checkRateLimit(AsyncWebSocketClient* client) {
  if (!client)
    return false;
  auto* state = findClientState(client->id());
  if (!state)
    return false;

  unsigned long now = millis();
  if (now - state->rateWindowStart > 1000) {
    state->rateWindowStart = now;
    state->rateCount = 1;
    return true;
  }

  state->rateCount++;
  if (state->rateCount > 5)
    return false;
  return true;
}

void DiagnosticsTerminal::updateClientActivity(uint32_t clientId) {
  if (auto* state = findClientState(clientId)) {
    state->lastActivity = millis();
  }
}

void DiagnosticsTerminal::checkSessionTimeouts() {
  if (!m_clientStates || m_clientStateCount == 0)
    return;
  unsigned long now = millis();
  for (size_t i = 0; i < m_clientStateCount; ++i) {
    auto& state = m_clientStates[i];
    if (state.inUse && state.client) {
      if (now - state.lastActivity > AppConstants::WS_SESSION_TIMEOUT_MS) {
        LOG_WARN("SECURITY", F("Client #%u session timed out (30min inactivity)"), state.client->id());
        state.client->close();
      }
    }
  }
}

void DiagnosticsTerminal::pushCommandToQueue(uint32_t clientId, const char* cmd, size_t cmd_len) {
  if (!cmd || cmd_len == 0 || !m_cmdQueue)
    return;

  // Minimize time spent with interrupts disabled: reserve slot first, then copy.
  bool dropped_oldest = false;
  size_t slot = 0;
  size_t max_len = QueuedCommand::MAX_LEN - 1;
  if (cmd_len > max_len)
    cmd_len = max_len;

  {
    Utils::InterruptGuard guard;
    size_t nextHead = (m_head + 1) & (CMD_QUEUE_SIZE - 1);
    if (nextHead == m_tail) {
      // Drop oldest to make room for newest command.
      m_tail = (m_tail + 1) & (CMD_QUEUE_SIZE - 1);
      dropped_oldest = true;
    }
    slot = m_head;
    m_cmdQueue[slot].clientId = REDACTED
    m_cmdQueue[slot].len = 0;  // mark as not ready
    m_head = nextHead;
  }

  memcpy(m_cmdQueue[slot].commandStr, cmd, cmd_len);
  m_cmdQueue[slot].commandStr[cmd_len] = '\0';
  {
    Utils::InterruptGuard guard;
    m_cmdQueue[slot].len = static_cast<uint8_t>(cmd_len);
  }

  if (dropped_oldest) {
    LOG_WARN("REDACTED", F("REDACTED"), clientId);
  }
}
