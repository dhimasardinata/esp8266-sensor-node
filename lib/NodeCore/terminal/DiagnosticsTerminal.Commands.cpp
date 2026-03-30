#include "terminal/DiagnosticsTerminal.h"

#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>
#include <cstring>
#include <new>
#include <stdarg.h>
#include <utility>

#include "commands/CommandContext.h"
#include "commands/ICommand.h"
#include "support/CryptoUtils.h"
#include "system/Logger.h"
#include "system/MemoryTelemetry.h"
#include "support/Utils.h"

// Command implementations - included for inline execution
#include "commands/CacheStatusCommand.h"
#include "commands/CheckUpdateCommand.h"
#include "commands/ClearCacheCommand.h"
#include "commands/CrashLogCommand.h"
#include "commands/FactoryResetCommand.h"
#include "REDACTED"
#include "commands/FormatFsCommand.h"
#include "commands/FsStatusCommand.h"
#include "commands/GetCalibrationCommand.h"
#include "commands/GetConfigCommand.h"
#include "commands/LoginCommand.h"
#include "commands/LogoutCommand.h"
#include "commands/ModeCommand.h"
#include "commands/NetConfigCommand.h"
#include "REDACTED"
#include "commands/QosCommand.h"
#include "commands/ReadSensorsCommand.h"
#include "commands/RebootCommand.h"
#include "commands/ResetCalibrationCommand.h"
#include "commands/SendNowCommand.h"
#include "commands/SetCalibrationCommand.h"
#include "commands/SetConfigCommand.h"
#include "commands/SetGatewayCommand.h"
#include "REDACTED"
#include "commands/SetTimeCommand.h"
#include "REDACTED"
#include "commands/UplinkCommand.h"
#include "commands/SetUrlCommand.h"
#include "REDACTED"
// HAPUS BARIS INI: #include "commands/StatusCommand.h"
#include "commands/SysInfoCommand.h"
#include "REDACTED"
#include "REDACTED"
#include "REDACTED"
#include "commands/ZeroCalibrationCommand.h"

// Includes untuk inline StatusCommand
#include "api/ApiClient.h"
#include "REDACTED"
#include "storage/CacheManager.h"
#include "net/NtpClient.h"
#include "storage/RtcManager.h"
#include "sensor/SensorManager.h"
#include "system/SystemHealth.h"
#include "terminal/TerminalFormatting.h"
#include "REDACTED"

// ============================================
// INLINE STATUS COMMAND IMPLEMENTATION
// ============================================
namespace {
  void sendHeapResetReport(AsyncWebSocketClient* client,
                           const MemoryTelemetry::HeapSnapshot& before,
                           const MemoryTelemetry::HeapSnapshot& after,
                           uint32_t watermarkFreeBefore,
                           uint32_t watermarkBlockBefore,
                           uint32_t watermarkFreeAfter,
                           uint32_t watermarkBlockAfter) {
    const MemoryTelemetry::HeapDeltaSummary summary = MemoryTelemetry::summarize(before, after);
    Utils::ws_printf_P(
        client,
        PSTR("[OK] Heap watermark reset\n"
             "  Current before  : free=%u B | maxBlock=%u B | frag=%u%%\n"
             "  Current after   : free=%u B | maxBlock=%u B | frag=%u%%\n"
             "  Memory freed    : %u B\n"
             "  Free heap delta : %ld B (%s)\n"
             "  Max block delta : %ld B (%s)\n"
             "  Watermark before: minFree=%u B | minBlock=%u B\n"
             "  Watermark after : minFree=%u B | minBlock=%u B\n"),
        before.freeHeap,
        before.maxBlock,
        before.fragmentation,
        after.freeHeap,
        after.maxBlock,
        after.fragmentation,
        MemoryTelemetry::positiveBytes(summary.delta.freeHeapBytes),
        static_cast<long>(summary.delta.freeHeapBytes),
        summary.freeHeapPercent,
        static_cast<long>(summary.delta.maxBlockBytes),
        summary.maxBlockPercent,
        watermarkFreeBefore,
        watermarkBlockBefore,
        watermarkFreeAfter,
        watermarkBlockAfter);
  }

  // Helper functions untuk status command
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

  void formatIp(char* out, size_t out_len, const IPAddress& ip) {
    if (!out || out_len == 0)
      return;
    size_t pos = 0;
    for (uint8_t i = 0; i < 4; ++i) {
      if (i > 0) {
        if (pos + 1 >= out_len)
          break;
        out[pos++] = '.';
      }
      pos += u32_to_dec(out + pos, (pos < out_len) ? (out_len - pos) : 0, ip[i]);
    }
    if (pos < out_len) {
      out[pos] = '\0';
    } else {
      out[out_len - 1] = '\0';
    }
  }

  void formatHms(char* out, size_t out_len, const tm& ti) {
    if (!out || out_len < 9) {
      if (out && out_len > 0) out[0] = '\0';
      return;
    }
    uint8_t h = static_cast<uint8_t>(ti.tm_hour);
    uint8_t m = static_cast<uint8_t>(ti.tm_min);
    uint8_t s = static_cast<uint8_t>(ti.tm_sec);
    out[0] = static_cast<char>('0' + (h / 10));
    out[1] = static_cast<char>('0' + (h % 10));
    out[2] = ':';
    out[3] = static_cast<char>('0' + (m / 10));
    out[4] = static_cast<char>('0' + (m % 10));
    out[5] = ':';
    out[6] = static_cast<char>('0' + (s / 10));
    out[7] = static_cast<char>('0' + (s % 10));
    out[8] = '\0';
  }

  class StatusPrinter {
  public:
    static constexpr size_t kStatusBufferSize = 1536;

    StatusPrinter(DiagnosticsTerminal& terminal, AsyncWebSocketClient* c)
        : m_terminal(terminal),
          m_client(c),
          m_buf(new (std::nothrow) char[kStatusBufferSize]),
          m_offset(0) {
      if (m_buf) {
        m_buf[0] = '\0';
      }
    }

    bool ready() const {
      return static_cast<bool>(m_buf);
    }

    void print(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
      if (!m_buf) {
        return;
      }
      va_list args;
      va_start(args, fmt);
      size_t avail = kStatusBufferSize - m_offset;
      int written = vsnprintf(m_buf.get() + m_offset, avail, fmt, args);
      va_end(args);
      if (written < 0)
        return;

      if (static_cast<size_t>(written) >= avail) {
        m_offset = kStatusBufferSize - 1;
        m_buf[m_offset] = '\0';
        return;
      }
      m_offset += static_cast<size_t>(written);
    }

    void print_P(PGM_P fmt, ...) __attribute__((format(printf, 2, 3))) {
      if (!m_buf) {
        return;
      }
      va_list args;
      va_start(args, fmt);
      size_t avail = kStatusBufferSize - m_offset;
      int written = vsnprintf_P(m_buf.get() + m_offset, avail, fmt, args);
      va_end(args);
      if (written < 0)
        return;

      if (static_cast<size_t>(written) >= avail) {
        m_offset = kStatusBufferSize - 1;
        m_buf[m_offset] = '\0';
        return;
      }
      m_offset += static_cast<size_t>(written);
    }

    void print(const __FlashStringHelper* text) {
      if (!text || !m_buf)
        return;

      PGM_P flashText = reinterpret_cast<PGM_P>(text);
      size_t totalLen = REDACTED
      size_t offset = 0;
      while (offset < totalLen) {
        if (m_offset >= kStatusBufferSize - 1) {
          break;
        }

        size_t avail = kStatusBufferSize - 1 - m_offset;
        size_t chunkLen = totalLen - offset;
        if (chunkLen > avail)
          chunkLen = avail;
        memcpy_P(m_buf.get() + m_offset, flashText + offset, chunkLen);
        m_offset += chunkLen;
        m_buf[m_offset] = '\0';
        offset += chunkLen;
      }
    }

    void flush() {
      if (!m_buf || m_offset == 0) {
        return;
      }

      std::unique_ptr<char[]> queued(new (std::nothrow) char[m_offset]);
      if (queued) {
        memcpy(queued.get(), m_buf.get(), m_offset);
        if (m_terminal.queueClientOutput(m_client->id(), std::move(queued), m_offset)) {
          m_offset = 0;
          m_buf[0] = '\0';
          return;
        }
      }
      Utils::ws_send_encrypted(m_client, std::string_view(m_buf.get(), m_offset));
      m_offset = 0;
      m_buf[0] = '\0';
    }

  private:
    DiagnosticsTerminal& m_terminal;
    AsyncWebSocketClient* m_client;
    std::unique_ptr<char[]> m_buf;
    size_t m_offset;
  };

  // Fungsi untuk execute status command
  void executeStatusCommand(const CommandContext& context,
                           DiagnosticsTerminal& terminal,
                           WifiManager& wifiManager,
                           NtpClient& ntpClient,
                           ApiClient& apiClient,
                           CacheManager& cacheManager,
                           SensorManager& sensorManager) {
    if (!context.client || !context.client->canSend())
      return;

    StatusPrinter p(terminal, context.client);
    if (!p.ready()) {
      Utils::ws_send_encrypted(context.client, F("[ERROR] Status output unavailable: low memory.\n"));
      return;
    }
    char uptime[32], ntpSince[32], apiSince[32], timeStr[32];
    strcpy_P(timeStr, PSTR("Not Synced"));

    TerminalFormat::formatUptime(uptime, sizeof(uptime), millis());
    TerminalFormat::formatTimeSince(ntpSince, sizeof(ntpSince), ntpClient.getLastSyncMillis());
    TerminalFormat::formatTimeSince(apiSince, sizeof(apiSince), apiClient.getLastSuccessMillis());

    if (ntpClient.isTimeSynced()) {
      time_t now = time(nullptr);
      struct tm ti;
      localtime_r(&now, &ti);
      formatHms(timeStr, sizeof(timeStr), ti);
    }

    // Get health metrics
    auto& health = SystemHealth::HealthMonitor::instance();
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    health.recordHeapSnapshot(freeHeap, maxBlock);
    int32_t rssi = WiFi.isConnected() ? WiFi.RSSI() : 0;
    auto score = health.calculateHealth(
        freeHeap, maxBlock, rssi, sensorManager.getShtStatus(), sensorManager.getBh1750Status());
    const auto& metrics = health.getLoopMetrics();
    char healthGrade[10];
    score.copyGrade(healthGrade, sizeof(healthGrade));

    p.print(F("\n========== SYSTEM STATUS ==========\n"));
    p.print_P(PSTR("FW: %s | Uptime: %s\n"), FIRMWARE_VERSION, uptime);

    // Health score with breakdown
    p.print_P(PSTR("\n[HEALTH] Score: %u/100 (%s)\n"), score.overall(), healthGrade);
    p.print_P(PSTR("  Heap:%u Frag:%u CPU:%u WiFi:%u Sensor:%u\n"),
              score.heap,
              score.fragmentation,
              score.cpu,
              score.wifi,
              score.sensor);

    // Memory details
    p.print(F("\n[MEMORY]\n"));
    p.print_P(PSTR("  Free: %u bytes | MaxBlock: %u bytes\n"), freeHeap, maxBlock);
    p.print_P(PSTR("  MinFree: %u bytes | MinBlock: %u bytes\n"),
              health.getMinFreeHeap(),
              health.getMinMaxBlock());
    p.print_P(PSTR("  Fragmentation: %d%%\n"), ESP.getHeapFragmentation());

    // CPU metrics
    p.print(F("\n[CPU]\n"));
    p.print_P(PSTR("  Loop avg: %lu us | max: %lu us\n"), metrics.getAverageDurationUs(), metrics.maxDurationUs);
    p.print_P(PSTR("  Slow loops: %u%% (%u total)\n"), metrics.getSlowLoopPercent(), metrics.slowLoopCount);

    // WiFi status
    WifiManager:REDACTED
    p.print(F("REDACTED"));
    if (ws == WifiManager::State::CONNECTED_STA) {
      p.print(F("Connected\n"));
    } else if (ws == WifiManager::State::PORTAL_MODE) {
      p.print(F("Portal\n"));
    } else {
      p.print(F("Disconnected\n"));
    }
    if (ws == WifiManager::State::CONNECTED_STA) {
      char ssid[WIFI_SSID_MAX_LEN] = REDACTED
      WiFi.SSID().toCharArray(ssid, sizeof(ssid));
      IPAddress ip = WiFi.localIP();
      char ipStr[16];
      formatIp(ipStr, sizeof(ipStr), ip);
      p.print_P(PSTR("  SSID: REDACTED
    } else if (ws == WifiManager::State::PORTAL_MODE) {
      IPAddress apIp = WiFi.softAPIP();
      char apIpStr[16];
      formatIp(apIpStr, sizeof(apIpStr), apIp);
      p.print_P(PSTR("  AP IP: %s\n"), apIpStr);
    }

    // Time and API
    char timeSource[8];
    char uploadMode[8];
    char gatewayState[10];
    ntpClient.copyTimeSourceLabel(timeSource, sizeof(timeSource));
    apiClient.copyUploadModeString(uploadMode, sizeof(uploadMode));
    strncpy_P(gatewayState,
              apiClient.isLocalGatewayActive() ? PSTR("active") : PSTR("inactive"),
              sizeof(gatewayState) - 1);
    gatewayState[sizeof(gatewayState) - 1] = '\0';
    p.print_P(PSTR("\n[TIME] %s (sync: %s ago, src: %s)\n"),
              timeStr,
              ntpSince,
              timeSource);
    p.print_P(PSTR("[API] Last success: %s ago\n"), apiSince);
    p.print_P(PSTR("[MODE] Upload: %s | Gateway: %s\n"), uploadMode, gatewayState);
    p.print_P(PSTR("[CACHE] RTC: %u/%u | LittleFS: %lu/%lu B\n"),
              static_cast<unsigned>(RtcManager::getCount()),
              static_cast<unsigned>(RTC_MAX_RECORDS),
              static_cast<unsigned long>(cacheManager.get_size()),
              static_cast<unsigned long>(MAX_CACHE_DATA_SIZE));
    p.print_P(PSTR("[CACHE] Emergency: %u/%u | Backpressure: "),
              static_cast<unsigned>(apiClient.getEmergencyQueueDepth()),
              static_cast<unsigned>(apiClient.getEmergencyQueueCapacity()));
    p.print(apiClient.isEmergencyBackpressureActive() ? F("ON\n") : F("OFF\n"));
    p.print(F("[WS] Utility enabled: "));
    p.print(Utils::ws_is_enabled() ? F("YES\n") : F("NO\n"));

    // Sensors
    p.print(F("[SENSORS] SHT: "));
    p.print(sensorManager.getShtStatus() ? F("OK") : F("FAIL"));
    p.print(F(" | BH1750: "));
    p.print(sensorManager.getBh1750Status() ? F("OK\n") : F("FAIL\n"));

    // Reboot Info
    BootGuard::RebootReason reason = BootGuard::getLastRebootReason();
    PGM_P reasonStr = PSTR("Unknown");
    switch (reason) {
      case BootGuard::RebootReason::POWER_ON:
        reasonStr = PSTR("Power On");
        break;
      case BootGuard::RebootReason::HW_WDT:
        reasonStr = PSTR("Hardware WDT");
        break;
      case BootGuard::RebootReason::EXCEPTION:
        reasonStr = PSTR("Crash/Exception");
        break;
      case BootGuard::RebootReason::SOFT_WDT:
        reasonStr = PSTR("Software WDT");
        break;
      case BootGuard::RebootReason::SOFT_RESTART:
        reasonStr = PSTR("Soft Restart");
        break;
      case BootGuard::RebootReason::DEEP_SLEEP:
        reasonStr = PSTR("Deep Sleep");
        break;
      case BootGuard::RebootReason::OTA_UPDATE:
        reasonStr = PSTR("OTA Update");
        break;
      case BootGuard::RebootReason::FACTORY_RESET:
        reasonStr = PSTR("Factory Reset");
        break;
      case BootGuard::RebootReason::HEALTH_CHECK:
        reasonStr = PSTR("Health Check");
        break;
      case BootGuard::RebootReason::CONFIG_CHANGE:
        reasonStr = PSTR("Config Change");
        break;
      case BootGuard::RebootReason::COMMAND:
        reasonStr = PSTR("Remote Command");
        break;
      default:
        reasonStr = PSTR("Unknown");
        break;
    }
    char reasonBuf[24];
    strncpy_P(reasonBuf, reasonStr, sizeof(reasonBuf) - 1);
    reasonBuf[sizeof(reasonBuf) - 1] = '\0';
    p.print_P(PSTR("[BOOT] Reason: %s | Crash Count: %u\n"), reasonBuf, BootGuard::getCrashCount());

    p.print(F("====================================\n"));
    p.flush();
  }

  void renderTerminalHelp(AsyncWebSocketClient* client,
                          bool isAuth,
                          TerminalServices& services,
                          DiagnosticsTerminal& terminal);

  class StatusBuiltinCommand final : public ICommand {
  public:
    StatusBuiltinCommand(DiagnosticsTerminal& terminal, TerminalServices& services)
        : m_terminal(terminal), m_services(services) {}

    PGM_P getName_P() const override { return PSTR("status"); }
    PGM_P getDescription_P() const override { return PSTR("Show system status"); }
    CommandSection helpSection() const override { return CommandSection::PUBLIC; }
    bool requiresAuth() const override { return false; }

    void execute(const CommandContext& context) override {
      executeStatusCommand(context,
                           m_terminal,
                           m_services.wifiManager,
                           m_services.ntpClient,
                           m_services.apiClient,
                           m_services.cacheManager,
                           m_services.sensorManager);
    }

  private:
    DiagnosticsTerminal& m_terminal;
    TerminalServices& m_services;
  };

  class HelpBuiltinCommand final : public ICommand {
  public:
    HelpBuiltinCommand(DiagnosticsTerminal& terminal, TerminalServices& services)
        : m_terminal(terminal), m_services(services) {}

    PGM_P getName_P() const override { return PSTR("help"); }
    PGM_P getDescription_P() const override { return PSTR("Show this help"); }
    CommandSection helpSection() const override { return CommandSection::PUBLIC; }
    bool requiresAuth() const override { return false; }

    void execute(const CommandContext& context) override {
      renderTerminalHelp(context.client, context.isAuthenticated, m_services, m_terminal);
    }

  private:
    DiagnosticsTerminal& m_terminal;
    TerminalServices& m_services;
  };

  class HeapResetBuiltinCommand final : public ICommand {
  public:
    PGM_P getName_P() const override { return PSTR("heapreset"); }
    PGM_P getDescription_P() const override { return PSTR("Reset heap watermarks"); }
    CommandSection helpSection() const override { return CommandSection::SYSTEM; }
    bool requiresAuth() const override { return true; }

    void execute(const CommandContext& context) override {
      if (!context.client || !context.client->canSend()) {
        return;
      }
      auto& health = SystemHealth::HealthMonitor::instance();
      const uint32_t watermarkFreeBefore = health.getMinFreeHeap();
      const uint32_t watermarkBlockBefore = health.getMinMaxBlock();
      const MemoryTelemetry::HeapSnapshot before = MemoryTelemetry::HeapSnapshot::capture();
      health.resetHeapWatermark(before.freeHeap, before.maxBlock);
      yield();
      const MemoryTelemetry::HeapSnapshot after = MemoryTelemetry::HeapSnapshot::capture();
      health.resetHeapWatermark(after.freeHeap, after.maxBlock);
      sendHeapResetReport(context.client,
                          before,
                          after,
                          watermarkFreeBefore,
                          watermarkBlockBefore,
                          health.getMinFreeHeap(),
                          health.getMinMaxBlock());
    }
  };
}  // namespace
// ============================================
// END INLINE STATUS COMMAND
// ============================================

void DiagnosticsTerminal::initCommands() {
  // No heap allocations needed - commands are dispatched via O(1) switch-case
  LOG_INFO("DIAG", F("Command dispatcher initialized (O(1) static dispatch)"));
}

namespace {
  template <typename Command, typename... Args>
  bool executeTerminalCommand(const CommandContext& ctx, bool isAuth, Args&&... args) {
    Command cmd(std::forward<Args>(args)...);
    if (cmd.requiresAuth() && !isAuth) {
      return false;
    }
    cmd.execute(ctx);
    return true;
  }
}


bool DiagnosticsTerminal::dispatchCommand(uint32_t cmdHash,
                                          const char* args,
                                          AsyncWebSocketClient* client,
                                          bool isAuth) {
  CommandContext ctx{args, client, isAuth};

  // O(1) dispatch using switch-case
  switch (cmdHash) {
    case CmdHash::CACHE: {
      return executeTerminalCommand<CacheStatusCommand>(
          ctx, isAuth, m_services.cacheManager, m_services.apiClient, m_services.configManager);
    }
    case CmdHash::CHECKUPDATE: {
      return executeTerminalCommand<CheckUpdateCommand>(
          ctx, isAuth, m_services.otaManager, m_services.wifiManager, m_services.ntpClient);
    }
    case CmdHash::CLEARCACHE: {
      return executeTerminalCommand<ClearCacheCommand>(ctx, isAuth, m_services.cacheManager);
    }
    case CmdHash::CLEARCRASH: {
      return executeTerminalCommand<ClearCrashCommand>(ctx, isAuth);
    }
    case CmdHash::CRASHLOG: {
      return executeTerminalCommand<CrashLogCommand>(ctx, isAuth);
    }
    case CmdHash::FACTORYRESET: {
      return executeTerminalCommand<FactoryResetCommand>(
          ctx, isAuth, m_services.configManager, m_services.cacheManager);
    }
    case CmdHash::FORMAT: {
      return executeTerminalCommand<FormatFsCommand>(ctx, isAuth, m_services.configManager);
    }
    case CmdHash::FSSTATUS: {
      return executeTerminalCommand<FsStatusCommand>(ctx, isAuth);
    }
    case CmdHash::GETCAL: {
      return executeTerminalCommand<GetCalibrationCommand>(ctx, isAuth, m_services.configManager);
    }
    case CmdHash::GETCONFIG: {
      return executeTerminalCommand<GetConfigCommand>(ctx, isAuth, m_services.configManager);
    }
    case CmdHash::NETCONFIG: {
      return executeTerminalCommand<NetConfigCommand>(ctx, isAuth, m_services.configManager, m_services.apiClient);
    }
    case CmdHash::LOGIN: {
      return executeTerminalCommand<LoginCommand>(ctx, isAuth, m_services.configManager, *this);
    }
    case CmdHash::LOGOUT: {
      return executeTerminalCommand<LogoutCommand>(ctx, isAuth, *this);
    }
    case CmdHash::QOSUPLOAD: {
      return executeTerminalCommand<QosUploadCommand>(ctx, isAuth, m_services.apiClient);
    }
    case CmdHash::QOSOTA: {
      return executeTerminalCommand<QosOtaCommand>(ctx, isAuth, m_services.apiClient);
    }
    case CmdHash::OPENWIFI: {
      return executeTerminalCommand<OpenWifiCommand>(ctx, isAuth, m_services.wifiManager);
    }
    case CmdHash::READ: {
      return executeTerminalCommand<ReadSensorsCommand>(
          ctx, isAuth, m_services.sensorManager, m_services.configManager);
    }
    case CmdHash::REBOOT: {
      return executeTerminalCommand<RebootCommand>(ctx, isAuth);
    }
    case CmdHash::RESETCAL: {
      return executeTerminalCommand<ResetCalibrationCommand>(ctx, isAuth, m_services.configManager);
    }
    case CmdHash::SENDNOW: {
      return executeTerminalCommand<SendNowCommand>(ctx, isAuth, m_services.apiClient);
    }
    case CmdHash::SETCAL: {
      return executeTerminalCommand<SetCalibrationCommand>(ctx, isAuth, m_services.configManager);
    }
    case CmdHash::SETCONFIG: {
      return executeTerminalCommand<SetConfigCommand>(ctx, isAuth, m_services.configManager);
    }
    case CmdHash::SETGATEWAY: {
      return executeTerminalCommand<SetGatewayCommand>(ctx, isAuth, m_services.configManager);
    }
    case CmdHash::SETPORTALPASS: {
      return executeTerminalCommand<SetPortalPassCommand>(ctx, isAuth, m_services.configManager);
    }
    case CmdHash::SETTOKEN: {
      return executeTerminalCommand<SetTokenCommand>(ctx, isAuth, m_services.configManager);
    }
    case CmdHash::SETURL: {
      return executeTerminalCommand<SetUrlCommand>(ctx, isAuth, m_services.configManager);
    }
    case CmdHash::SETTIME: {
      return executeTerminalCommand<SetTimeCommand>(ctx, isAuth, m_services.ntpClient);
    }
    case CmdHash::SETWIFI: {
      return executeTerminalCommand<SetWifiCommand>(ctx, isAuth, m_services.configManager);
    }
    case CmdHash::STATUS: {
      return executeTerminalCommand<StatusBuiltinCommand>(ctx, isAuth, *this, m_services);
    }
    case CmdHash::SYSINFO: {
      return executeTerminalCommand<SysInfoCommand>(ctx, isAuth);
    }
    case CmdHash::WIFILIST: {
      return executeTerminalCommand<WifiListCommand>(ctx, isAuth, m_services.wifiManager);
    }
    case CmdHash::WIFIADD: {
      return executeTerminalCommand<WifiAddCommand>(ctx, isAuth, m_services.wifiManager);
    }
    case CmdHash::WIFIREMOVE: {
      return executeTerminalCommand<WifiRemoveCommand>(ctx, isAuth, m_services.wifiManager);
    }
    case CmdHash::ZEROCAL: {
      return executeTerminalCommand<ZeroCalibrationCommand>(ctx, isAuth, m_services.configManager);
    }
    case CmdHash::MODE: {
      return executeTerminalCommand<ModeCommand>(ctx, isAuth, m_services.apiClient);
    }
    case CmdHash::UPLINK: {
      return executeTerminalCommand<UplinkCommand>(ctx, isAuth, m_services.configManager, m_services.apiClient);
    }
    case CmdHash::FORCEOTAINSECURE: {
      return executeTerminalCommand<ForceOtaInsecureCommand>(ctx, isAuth, m_services.otaManager);
    }
    case CmdHash::HEAPRESET: {
      return executeTerminalCommand<HeapResetBuiltinCommand>(ctx, isAuth);
    }
    case CmdHash::HELP: {
      return executeTerminalCommand<HelpBuiltinCommand>(ctx, isAuth, *this, m_services);
    }
    default:
      return false;  // Unknown command
  }
}


void DiagnosticsTerminal::printHelp(AsyncWebSocketClient* client, bool isAuth) {
  renderTerminalHelp(client, isAuth, m_services, *this);
}


namespace {
void renderTerminalHelp(AsyncWebSocketClient* client,
                        bool isAuth,
                        TerminalServices& services,
                        DiagnosticsTerminal& terminal) {
  if (!client || !client->canSend()) {
    return;
  }

  auto sectionTitle = [](CommandSection section) -> PGM_P {
    switch (section) {
      case CommandSection::PUBLIC:
        return PSTR("[Public]");
      case CommandSection::SENSORS_DATA:
        return PSTR("[Sensors & Data]");
      case CommandSection::CALIBRATION:
        return PSTR("[Calibration]");
      case CommandSection::CONFIGURATION:
        return PSTR("[Configuration]");
      case CommandSection::WIFI:
        return PSTR("REDACTED");
      case CommandSection::SYSTEM:
      default:
        return PSTR("[System]");
    }
  };

  StatusBuiltinCommand status(terminal, services);
  HelpBuiltinCommand help(terminal, services);
  HeapResetBuiltinCommand heapReset;
  LoginCommand login(services.configManager, terminal);
  LogoutCommand logout(terminal);
  SysInfoCommand sysInfo;
  CacheStatusCommand cacheStatus(services.cacheManager, services.apiClient, services.configManager);
  ClearCacheCommand clearCache(services.cacheManager);
  ReadSensorsCommand readSensors(services.sensorManager, services.configManager);
  SendNowCommand sendNow(services.apiClient);
  GetCalibrationCommand getCalibration(services.configManager);
  SetCalibrationCommand setCalibration(services.configManager);
  ZeroCalibrationCommand zeroCalibration(services.configManager);
  ResetCalibrationCommand resetCalibration(services.configManager);
  GetConfigCommand getConfig(services.configManager);
  NetConfigCommand netConfig(services.configManager, services.apiClient);
  SetConfigCommand setConfig(services.configManager);
  SetGatewayCommand setGateway(services.configManager);
  SetTokenCommand setToken(services.configManager);
  SetUrlCommand setUrl(services.configManager);
  SetTimeCommand setTime(services.ntpClient);
  SetWifiCommand setWifi(services.configManager);
  SetPortalPassCommand setPortalPass(services.configManager);
  WifiListCommand wifiList(services.wifiManager);
  WifiAddCommand wifiAdd(services.wifiManager);
  WifiRemoveCommand wifiRemove(services.wifiManager);
  OpenWifiCommand openWifi(services.wifiManager);
  CheckUpdateCommand checkUpdate(services.otaManager, services.wifiManager, services.ntpClient);
  CrashLogCommand crashLog;
  ClearCrashCommand clearCrash;
  FsStatusCommand fsStatus;
  ModeCommand mode(services.apiClient);
  UplinkCommand uplink(services.configManager, services.apiClient);
  QosUploadCommand qosUpload(services.apiClient);
  QosOtaCommand qosOta(services.apiClient);
  RebootCommand reboot;
  FactoryResetCommand factoryReset(services.configManager, services.cacheManager);
  FormatFsCommand formatFs(services.configManager);
  ForceOtaInsecureCommand forceOtaInsecure(services.otaManager);

  const ICommand* commands[] = {
      &status,           &sysInfo,          &login,         &logout,         &help,
      &readSensors,      &sendNow,          &cacheStatus,   &clearCache,     &getCalibration,
      &setCalibration,   &zeroCalibration,  &resetCalibration, &getConfig,   &netConfig,
      &setConfig,
      &setGateway,       &setToken,         &setUrl,           &setTime,       &setWifi,
      &setPortalPass,    &wifiList,
      &wifiAdd,          &wifiRemove,       &openWifi,      &checkUpdate,    &crashLog,
      &clearCrash,       &fsStatus,         &mode,          &uplink,         &qosUpload,      &qosOta,
      &reboot,           &factoryReset,     &formatFs,      &forceOtaInsecure, &heapReset,
  };

  auto sectionHasVisibleEntries = [&](CommandSection section) {
    for (const ICommand* cmd : commands) {
      if (cmd->helpSection() == section && (isAuth || !cmd->requiresAuth())) {
        return true;
      }
    }
    return false;
  };

  auto appendLen = [](size_t& total, PGM_P text_P) {
    total += REDACTED
  };

  auto appendEntryLen = [&](size_t& total, PGM_P name_P, PGM_P desc_P) {
    appendLen(total, PSTR("REDACTED"));
    appendLen(total, name_P);
    appendLen(total, PSTR("REDACTED"));
    appendLen(total, desc_P);
    appendLen(total, PSTR("REDACTED"));
  };

  auto sectionLen = [&](CommandSection section) -> size_t {
    if (!sectionHasVisibleEntries(section)) {
      return 0;
    }

    size_t total = REDACTED
    appendLen(total, PSTR("REDACTED"));
    appendLen(total, sectionTitle(section));
    appendLen(total, PSTR("REDACTED"));
    for (const ICommand* cmd : commands) {
      if (cmd->helpSection() == section && (isAuth || !cmd->requiresAuth())) {
        appendEntryLen(total, cmd->getName_P(), cmd->getDescription_P());
      }
    }
    return total;
  };

  size_t totalLen = REDACTED
  appendLen(totalLen, PSTR("REDACTED"));
  totalLen += REDACTED

  if (isAuth) {
    totalLen += REDACTED
    totalLen += REDACTED
    totalLen += REDACTED
    totalLen += REDACTED
    totalLen += REDACTED
  } else {
    appendLen(totalLen, PSTR("REDACTED"));
  }
  appendLen(totalLen, PSTR("REDACTED"));

  std::unique_ptr<char[]> helpText(new (std::nothrow) char[totalLen + 1]);
  if (!helpText) {
    Utils::ws_send_encrypted(client, F("\n[ERROR] Help output unavailable: low memory.\n"));
    return;
  }

  size_t pos = 0;
  auto appendToBuffer = [&](PGM_P text_P) {
    const size_t len = strlen_P(text_P);
    memcpy_P(helpText.get() + pos, text_P, len);
    pos += len;
  };

  auto appendEntry = [&](PGM_P name_P, PGM_P desc_P) {
    appendToBuffer(PSTR("  "));
    appendToBuffer(name_P);
    appendToBuffer(PSTR(" - "));
    appendToBuffer(desc_P);
    appendToBuffer(PSTR("\n"));
  };

  auto appendSection = [&](CommandSection section) {
    if (!sectionHasVisibleEntries(section)) {
      return;
    }

    appendToBuffer(PSTR("\n"));
    appendToBuffer(sectionTitle(section));
    appendToBuffer(PSTR("\n"));
    for (const ICommand* cmd : commands) {
      if (cmd->helpSection() == section && (isAuth || !cmd->requiresAuth())) {
        appendEntry(cmd->getName_P(), cmd->getDescription_P());
      }
    }
  };

  appendToBuffer(PSTR("\n--- Available Commands ---\n"));
  appendSection(CommandSection::PUBLIC);

  if (isAuth) {
    appendSection(CommandSection::SENSORS_DATA);
    appendSection(CommandSection::CALIBRATION);
    appendSection(CommandSection::CONFIGURATION);
    appendSection(CommandSection::WIFI);
    appendSection(CommandSection::SYSTEM);
  } else {
    appendToBuffer(PSTR("REDACTED"));
  }
  appendToBuffer(PSTR("--------------------------\n"));
  helpText[pos] = '\0';

  if (!terminal.queueClientOutput(client->id(), std::move(helpText), pos)) {
    Utils::ws_send_encrypted(client, F("\n[ERROR] Help output unavailable: low memory.\n"));
  }
}
}  // namespace
