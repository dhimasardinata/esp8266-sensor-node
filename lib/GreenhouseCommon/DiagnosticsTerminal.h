#ifndef DIAGNOSTICS_TERMINAL_H
#define DIAGNOSTICS_TERMINAL_H

#include <array>
#include <memory>
#include <string_view>
#include <AsyncWebSocket.h>

#include "CompileTimeUtils.h"
#include "CryptoUtils.h"
#include "REDACTED"
#include "IntervalTimer.h"
#include "constants.h"

// Forward declarations
class ConfigManager;
class WifiManager;
class NtpClient;
class SensorManager;
class CacheManager;
class ApiClient;
class OtaManager;

struct TerminalServices {
  ConfigManager& configManager;
  WifiManager& wifiManager;
  NtpClient& ntpClient;
  SensorManager& sensorManager;
  CacheManager& cacheManager;
  ApiClient& apiClient;
  OtaManager& otaManager;
};

// Compile-time command hash constants for O(1) dispatch
namespace CmdHash {
  constexpr uint32_t CACHE = CompileTimeUtils::ct_hash("cache");
  constexpr uint32_t CHECKUPDATE = CompileTimeUtils::ct_hash("checkupdate");
  constexpr uint32_t CLEARCACHE = CompileTimeUtils::ct_hash("clearcache");
  constexpr uint32_t CLEARCRASH = CompileTimeUtils::ct_hash("clearcrash");
  constexpr uint32_t CRASHLOG = CompileTimeUtils::ct_hash("crashlog");
  constexpr uint32_t FACTORYRESET = CompileTimeUtils::ct_hash("factoryreset");
  constexpr uint32_t FORMAT = CompileTimeUtils::ct_hash("format");
  constexpr uint32_t FSSTATUS = CompileTimeUtils::ct_hash("fsstatus");
  constexpr uint32_t GETCAL = CompileTimeUtils::ct_hash("getcal");
  constexpr uint32_t GETCONFIG = CompileTimeUtils::ct_hash("getconfig");
  constexpr uint32_t LOGIN = CompileTimeUtils::ct_hash("login");
  constexpr uint32_t LOGOUT = CompileTimeUtils::ct_hash("logout");
  constexpr uint32_t QOSUPLOAD = CompileTimeUtils::ct_hash("qosupload");
  constexpr uint32_t QOSOTA = REDACTED
  constexpr uint32_t OPENWIFI = REDACTED
  constexpr uint32_t READ = CompileTimeUtils::ct_hash("read");
  constexpr uint32_t REBOOT = CompileTimeUtils::ct_hash("reboot");
  constexpr uint32_t RESETCAL = CompileTimeUtils::ct_hash("resetcal");
  constexpr uint32_t SENDNOW = CompileTimeUtils::ct_hash("sendnow");
  constexpr uint32_t SETCAL = CompileTimeUtils::ct_hash("setcal");
  constexpr uint32_t SETCONFIG = CompileTimeUtils::ct_hash("setconfig");
  constexpr uint32_t SETPORTALPASS = REDACTED
  constexpr uint32_t SETTOKEN = REDACTED
  constexpr uint32_t SETWIFI = REDACTED
  constexpr uint32_t STATUS = CompileTimeUtils::ct_hash("status");
  constexpr uint32_t SYSINFO = CompileTimeUtils::ct_hash("sysinfo");
  constexpr uint32_t WIFILIST = REDACTED
  constexpr uint32_t WIFIADD = REDACTED
  constexpr uint32_t WIFIREMOVE = REDACTED
  constexpr uint32_t ZEROCAL = CompileTimeUtils::ct_hash("zerocal");
  constexpr uint32_t MODE = CompileTimeUtils::ct_hash("mode");
  constexpr uint32_t HELP = CompileTimeUtils::ct_hash("help");
  constexpr uint32_t FORCEOTAINSECURE = REDACTED
}  // namespace CmdHash

class DiagnosticsTerminal : public IAuthManager<DiagnosticsTerminal> {
  friend class IAuthManager<DiagnosticsTerminal>;

public:
  DiagnosticsTerminal(AsyncWebSocket& ws, TerminalServices& services);

  DiagnosticsTerminal(const DiagnosticsTerminal&) = delete;
  DiagnosticsTerminal& operator=(const DiagnosticsTerminal&) = delete;
  ~DiagnosticsTerminal();

  void init();
  void handle();
  bool setEnabled(bool enabled);

  // IAuthManager CRTP implementation methods
  bool isClientAuthenticatedImpl(uint32_t clientId) const;
  void setClientAuthenticatedImpl(uint32_t clientId, bool authenticated);
  bool isClientLockedOutImpl(uint32_t clientId) const;
  void recordFailedLoginImpl(uint32_t clientId);
  void clearFailedLoginsImpl(uint32_t clientId);

private:
  void onEvent(AsyncWebSocketClient* client, int type, void* arg, uint8_t* data, size_t len);
  void handleConnect(AsyncWebSocketClient* client);
  bool isValidFrame(AwsFrameInfo* info, size_t len, AsyncWebSocketClient* client);
  void handleDataFrame(AsyncWebSocketClient* client, void* arg, uint8_t* data, size_t len);

  void initCommands();
  void pushCommandToQueue(uint32_t clientId, const char* cmd, size_t cmd_len);

  // O(1) dispatch using switch-case on compile-time hashes
  bool dispatchCommand(uint32_t cmdHash, const char* args, AsyncWebSocketClient* client, bool isAuth);
  void printHelp(AsyncWebSocketClient* client, bool isAuth);

  [[nodiscard]] bool isAuthenticated(AsyncWebSocketClient* client) const;

  void ws_println_client(AsyncWebSocketClient* client, const char* msg);
  void ws_println_client(AsyncWebSocketClient* client, const __FlashStringHelper* msg);

  AsyncWebSocket& m_ws;
  TerminalServices& m_services;

  // Consolidated client state into fixed-size array to reduce heap overhead.
  struct ClientState {
    AsyncWebSocketClient* client = nullptr;
    unsigned long lastActivity = 0;
    unsigned long lastFailMs = 0;
    unsigned long rateWindowStart = 0;
    uint8_t failedAttempts = 0;
    uint8_t rateCount = 0;
    bool isAuthenticated = REDACTED
    bool inUse = false;
  };
  std::unique_ptr<ClientState[]> m_clientStates;
  size_t m_clientStateCount = 0;

  ClientState* findClientState(uint32_t clientId);
  const ClientState* findClientState(uint32_t clientId) const;
  ClientState* allocateClientState(AsyncWebSocketClient* client);
  void freeClientState(uint32_t clientId);

  bool checkRateLimit(AsyncWebSocketClient* client);
  void updateClientActivity(uint32_t clientId);
  void checkSessionTimeouts();

  IntervalTimer m_sessionCheckTimer{AppConstants::WS_SESSION_CHECK_INTERVAL_MS};

  // Circular Command Queue (Fixed Size, Zero Allocation) for Thread Safety
  struct QueuedCommand {
    uint32_t clientId;
    uint8_t len = 0;
    static constexpr size_t MAX_LEN = 64;
    char commandStr[MAX_LEN];
  };
  static constexpr size_t CMD_QUEUE_SIZE = 2;
  static_assert((CMD_QUEUE_SIZE & (CMD_QUEUE_SIZE - 1)) == 0, "CMD_QUEUE_SIZE must be power of two");
  std::unique_ptr<QueuedCommand[]> m_cmdQueue;
  size_t m_cmdQueueSize = 0;
  volatile size_t m_head = 0;
  volatile size_t m_tail = 0;

  bool ensureBuffers();
  void releaseBuffers();

  // RX/decrypt buffers (allocated on demand to reduce idle RAM)
  std::unique_ptr<char[]> m_rxBuffer;
  std::unique_ptr<char[]> m_decBuffer;
  size_t m_rxBufferSize = 0;
  size_t m_decBufferSize = 0;
  bool m_buffersReady = false;
  bool m_rxBusy = false;
  size_t m_activeClientCount = 0;
};

#endif  // DIAGNOSTICS_TERMINAL_H
