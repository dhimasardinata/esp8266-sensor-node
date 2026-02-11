#include "DiagnosticsTerminal.h"

#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>
#include <stdarg.h>

#include "Commands/CommandContext.h"
#include "Commands/ICommand.h"
#include "CryptoUtils.h"
#include "Logger.h"
#include "utils.h"

// Command implementations - included for inline execution
#include "Commands/CacheStatusCommand.h"
#include "Commands/CheckUpdateCommand.h"
#include "Commands/ClearCacheCommand.h"
#include "Commands/CrashLogCommand.h"
#include "Commands/FactoryResetCommand.h"
#include "REDACTED"
#include "Commands/FormatFsCommand.h"
#include "Commands/FsStatusCommand.h"
#include "Commands/GetCalibrationCommand.h"
#include "Commands/GetConfigCommand.h"
#include "Commands/LoginCommand.h"
#include "Commands/LogoutCommand.h"
#include "Commands/ModeCommand.h"
#include "REDACTED"
#include "Commands/QosCommand.h"
#include "Commands/ReadSensorsCommand.h"
#include "Commands/RebootCommand.h"
#include "Commands/ResetCalibrationCommand.h"
#include "Commands/SendNowCommand.h"
#include "Commands/SetCalibrationCommand.h"
#include "Commands/SetConfigCommand.h"
#include "REDACTED"
#include "REDACTED"
#include "REDACTED"
// HAPUS BARIS INI: #include "Commands/StatusCommand.h"
#include "Commands/SysInfoCommand.h"
#include "REDACTED"
#include "REDACTED"
#include "REDACTED"
#include "Commands/ZeroCalibrationCommand.h"

// Includes untuk inline StatusCommand
#include "ApiClient.h"
#include "BootGuard.h"
#include "NtpClient.h"
#include "SensorManager.h"
#include "SystemHealth.h"
#include "TerminalFormatting.h"
#include "REDACTED"

// ============================================
// INLINE STATUS COMMAND IMPLEMENTATION
// ============================================
namespace {
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
    StatusPrinter(AsyncWebSocketClient* c) : m_client(c), m_offset(0) {
      memset(m_buf, 0, sizeof(m_buf));
    }

    void print(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
      va_list args;
      va_start(args, fmt);
      size_t avail = sizeof(m_buf) - m_offset;
      int written = vsnprintf(m_buf + m_offset, avail, fmt, args);
      va_end(args);
      if (written < 0)
        return;

      if (static_cast<size_t>(written) >= avail) {
        flush();
        va_start(args, fmt);
        written = vsnprintf(m_buf, sizeof(m_buf), fmt, args);
        va_end(args);
        if (written < 0)
          return;
        m_offset = (static_cast<size_t>(written) >= sizeof(m_buf)) ? (sizeof(m_buf) - 1) : static_cast<size_t>(written);
      } else {
        m_offset += static_cast<size_t>(written);
      }
    }

    void flush() {
      if (m_offset > 0) {
        Utils::ws_send_encrypted(m_client, std::string_view(m_buf, m_offset));
        m_offset = 0;
        m_buf[0] = '\0';
      }
    }

  private:
    AsyncWebSocketClient* m_client;
    char m_buf[384];
    size_t m_offset;
  };

  // Fungsi untuk execute status command
  void executeStatusCommand(const CommandContext& context,
                           WifiManager& wifiManager,
                           NtpClient& ntpClient,
                           ApiClient& apiClient,
                           SensorManager& sensorManager) {
    if (!context.client || !context.client->canSend())
      return;

    StatusPrinter p(context.client);
    char uptime[32], ntpSince[32], apiSince[32], timeStr[32] = "Not Synced";

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
    int32_t rssi = WiFi.isConnected() ? WiFi.RSSI() : 0;
    auto score = health.calculateHealth(
        freeHeap, maxBlock, rssi, sensorManager.getShtStatus(), sensorManager.getBh1750Status());
    const auto& metrics = health.getLoopMetrics();

    p.print("\n========== SYSTEM STATUS ==========\n");
    p.print("FW: %s | Uptime: %s\n", FIRMWARE_VERSION, uptime);

    // Health score with breakdown
    p.print("\n[HEALTH] Score: %u/100 (%s)\n", score.overall(), score.getGrade());
    p.print("  Heap:%u Frag:%u CPU:%u WiFi:%u Sensor:%u\n",
            score.heap,
            score.fragmentation,
            score.cpu,
            score.wifi,
            score.sensor);

    // Memory details
    p.print("\n[MEMORY]\n");
    p.print("  Free: %u bytes | MaxBlock: %u bytes\n", freeHeap, maxBlock);
    p.print("  Fragmentation: %d%%\n", ESP.getHeapFragmentation());

    // CPU metrics
    p.print("\n[CPU]\n");
    p.print("  Loop avg: %lu us | max: %lu us\n", metrics.getAverageDurationUs(), metrics.maxDurationUs);
    p.print("  Slow loops: %u%% (%u total)\n", metrics.getSlowLoopPercent(), metrics.slowLoopCount);

    // WiFi status
    WifiManager:REDACTED
    p.print("REDACTED",
            ws == WifiManager::State::CONNECTED_STA
                ? "Connected"
                : (ws == WifiManager::State::PORTAL_MODE ? "Portal" : "Disconnected"));
    if (ws == WifiManager::State::CONNECTED_STA) {
      char ssid[WIFI_SSID_MAX_LEN] = REDACTED
      WiFi.SSID().toCharArray(ssid, sizeof(ssid));
      IPAddress ip = WiFi.localIP();
      char ipStr[16];
      formatIp(ipStr, sizeof(ipStr), ip);
      p.print("  SSID: REDACTED
    } else if (ws == WifiManager::State::PORTAL_MODE) {
      IPAddress apIp = WiFi.softAPIP();
      char apIpStr[16];
      formatIp(apIpStr, sizeof(apIpStr), apIp);
      p.print("  AP IP: %s\n", apIpStr);
    }

    // Time and API
    p.print("\n[TIME] %s (sync: %s ago)\n", timeStr, ntpSince);
    p.print("[API] Last success: %s ago\n", apiSince);

    // Sensors
    p.print("[SENSORS] SHT: %s | BH1750: %s\n",
            sensorManager.getShtStatus() ? "OK" : "FAIL",
            sensorManager.getBh1750Status() ? "OK" : "FAIL");

    // Reboot Info
    BootGuard::RebootReason reason = BootGuard::getLastRebootReason();
    const char* reasonStr = "Unknown";
    switch (reason) {
      case BootGuard::RebootReason::POWER_ON:
        reasonStr = "Power On";
        break;
      case BootGuard::RebootReason::HW_WDT:
        reasonStr = "Hardware WDT";
        break;
      case BootGuard::RebootReason::EXCEPTION:
        reasonStr = "Crash/Exception";
        break;
      case BootGuard::RebootReason::SOFT_WDT:
        reasonStr = "Software WDT";
        break;
      case BootGuard::RebootReason::SOFT_RESTART:
        reasonStr = "Soft Restart";
        break;
      case BootGuard::RebootReason::DEEP_SLEEP:
        reasonStr = "Deep Sleep";
        break;
      case BootGuard::RebootReason::OTA_UPDATE:
        reasonStr = "OTA Update";
        break;
      case BootGuard::RebootReason::FACTORY_RESET:
        reasonStr = "Factory Reset";
        break;
      case BootGuard::RebootReason::HEALTH_CHECK:
        reasonStr = "Health Check";
        break;
      case BootGuard::RebootReason::CONFIG_CHANGE:
        reasonStr = "Config Change";
        break;
      case BootGuard::RebootReason::COMMAND:
        reasonStr = "Remote Command";
        break;
      default:
        reasonStr = "Unknown";
        break;
    }
    p.print("[BOOT] Reason: %s | Crash Count: %u\n", reasonStr, BootGuard::getCrashCount());

    p.print("====================================\n");
    p.flush();
  }
}  // namespace
// ============================================
// END INLINE STATUS COMMAND
// ============================================

void DiagnosticsTerminal::initCommands() {
  // No heap allocations needed - commands are dispatched via O(1) switch-case
  LOG_INFO("DIAG", F("Command dispatcher initialized (O(1) static dispatch)"));
}


bool DiagnosticsTerminal::dispatchCommand(uint32_t cmdHash,
                                          const char* args,
                                          AsyncWebSocketClient* client,
                                          bool isAuth) {
  CommandContext ctx{args, client, isAuth};

  // O(1) dispatch using switch-case
  switch (cmdHash) {
    case CmdHash::CACHE: {
      if (!isAuth)
        return false;
      CacheStatusCommand cmd(m_services.cacheManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::CHECKUPDATE: {
      if (!isAuth)
        return false;
      CheckUpdateCommand cmd(m_services.otaManager, m_services.wifiManager, m_services.ntpClient);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::CLEARCACHE: {
      if (!isAuth)
        return false;
      ClearCacheCommand cmd(m_services.cacheManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::CLEARCRASH: {
      if (!isAuth)
        return false;
      ClearCrashCommand cmd;
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::CRASHLOG: {
      if (!isAuth)
        return false;
      CrashLogCommand cmd;
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::FACTORYRESET: {
      if (!isAuth)
        return false;
      FactoryResetCommand cmd(m_services.configManager, m_services.cacheManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::FORMAT: {
      if (!isAuth)
        return false;
      FormatFsCommand cmd(m_services.configManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::FSSTATUS: {
      if (!isAuth)
        return false;
      FsStatusCommand cmd;
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::GETCAL: {
      if (!isAuth)
        return false;
      GetCalibrationCommand cmd(m_services.configManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::GETCONFIG: {
      if (!isAuth)
        return false;
      GetConfigCommand cmd(m_services.configManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::LOGIN: {
      // Login doesn't require auth
      LoginCommand cmd(m_services.configManager, *this);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::LOGOUT: {
      // Logout doesn't require auth
      LogoutCommand cmd(*this);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::QOSUPLOAD: {
      if (!isAuth)
        return false;
      QosUploadCommand cmd(m_services.apiClient);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::QOSOTA: {
      if (!isAuth)
        return false;
      QosOtaCommand cmd(m_services.apiClient);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::OPENWIFI: {
      if (!isAuth)
        return false;
      OpenWifiCommand cmd(m_services.wifiManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::READ: {
      if (!isAuth)
        return false;
      ReadSensorsCommand cmd(m_services.sensorManager, m_services.configManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::REBOOT: {
      if (!isAuth)
        return false;
      RebootCommand cmd;
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::RESETCAL: {
      if (!isAuth)
        return false;
      ResetCalibrationCommand cmd(m_services.configManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::SENDNOW: {
      if (!isAuth)
        return false;
      SendNowCommand cmd(m_services.apiClient);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::SETCAL: {
      if (!isAuth)
        return false;
      SetCalibrationCommand cmd(m_services.configManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::SETCONFIG: {
      if (!isAuth)
        return false;
      SetConfigCommand cmd(m_services.configManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::SETPORTALPASS: {
      if (!isAuth)
        return false;
      SetPortalPassCommand cmd(m_services.configManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::SETTOKEN: {
      if (!isAuth)
        return false;
      SetTokenCommand cmd(m_services.configManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::SETWIFI: {
      if (!isAuth)
        return false;
      SetWifiCommand cmd(m_services.configManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::STATUS: {
      // Status is public - GUNAKAN FUNGSI INLINE
      executeStatusCommand(ctx, m_services.wifiManager, m_services.ntpClient, m_services.apiClient, m_services.sensorManager);
      return true;
    }
    case CmdHash::SYSINFO: {
      // SysInfo is public
      SysInfoCommand cmd;
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::WIFILIST: {
      if (!isAuth)
        return false;
      WifiListCommand cmd(m_services.wifiManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::WIFIADD: {
      if (!isAuth)
        return false;
      WifiAddCommand cmd(m_services.wifiManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::WIFIREMOVE: {
      if (!isAuth)
        return false;
      WifiRemoveCommand cmd(m_services.wifiManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::ZEROCAL: {
      if (!isAuth)
        return false;
      ZeroCalibrationCommand cmd(m_services.configManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::MODE: {
      if (!isAuth)
        return false;
      ModeCommand cmd(m_services.apiClient);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::FORCEOTAINSECURE: {
      if (!isAuth)
        return false;
      ForceOtaInsecureCommand cmd(m_services.otaManager);
      cmd.execute(ctx);
      return true;
    }
    case CmdHash::HELP: {
      // Help is public
      printHelp(client, isAuth);
      return true;
    }
    default:
      return false;  // Unknown command
  }
}


void DiagnosticsTerminal::printHelp(AsyncWebSocketClient* client, bool isAuth) {
  char buf[256];
  char* p = buf;
  size_t remaining = sizeof(buf);

  auto append = [&](const char* text) {
    size_t len = strnlen(text, remaining);
    if (len < remaining) {
      memcpy(p, text, len);
      p += len;
      remaining -= len;
    }
  };

  auto append_P = [&](const char* text_P) {
    size_t len = strlen_P(text_P);
    if (len < remaining) {
      memcpy_P(p, text_P, len);
      p += len;
      remaining -= len;
    }
  };

  auto flush = [&]() {
    if (p > buf) {
      *p = '\0';
      Utils::ws_send_encrypted(client, std::string_view(buf, static_cast<size_t>(p - buf)));
      p = buf;
      remaining = sizeof(buf);
    }
  };

  append_P(PSTR("\n--- Available Commands ---\n"));
  append_P(PSTR("\n[Public]\n"));
  append("  status      - Show system status\n");
  append("  sysinfo     - Show system info\n");
  append("REDACTED");
  append("  logout      - End session\n");
  append("  help        - Show this help\n");
  flush();

  if (isAuth) {
    append_P(PSTR("\n[Sensors & Data]\n"));
    append("  read        - Read sensor values\n");
    append("  sendnow     - Force data upload\n");
    append("  cache       - Show cache status\n");
    append("  clearcache  - Clear data cache\n");
    flush();

    append_P(PSTR("\n[Calibration]\n"));
    append("  getcal      - Show calibration\n");
    append("  setcal <s>  - Set calibration\n");
    append("  zerocal     - Zero calibration\n");
    append("  resetcal    - Reset calibration\n");
    flush();

    append_P(PSTR("\n[Configuration]\n"));
    append("  getconfig   - Show config\n");
    append("  setconfig   - Set config\n");
    append("REDACTED");
    append("REDACTED");
    append("REDACTED");
    flush();

    append_P(PSTR("REDACTED"));
    append("REDACTED");
    append("REDACTED");
    append("REDACTED");
    append("REDACTED");
    flush();

    append_P(PSTR("\n[System]\n"));
    append("  checkupdate - Check for firmware\n");
    append("  crashlog    - Show crash log\n");
    append("  clearcrash  - Clear crash log\n");
    append("  fsstatus    - Filesystem status\n");
    append("  mode <m>    - Set upload mode\n");
    append("  qosupload   - QoS upload test\n");
    append("REDACTED");
    append("  reboot      - Restart device\n");
    append("  factoryreset- Factory reset\n");
    append("  format      - Format filesystem\n");
    flush();
  } else {
    append_P(PSTR("REDACTED"));
  }
  append_P(PSTR("--------------------------\n"));
  flush();
}