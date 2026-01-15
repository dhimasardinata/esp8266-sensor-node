#include "StatusCommand.h"

#include <ESP8266WiFi.h>
#include <stdarg.h>

#include "ApiClient.h"
#include "ISensorManager.h"
#include "NtpClient.h"
#include "TerminalFormatting.h"
#include "WifiManager.h"
#include "utils.h"

StatusCommand::StatusCommand(WifiManager& wifiManager, NtpClient& ntpClient,
                             ApiClient& apiClient, ISensorManager& sensorManager)
    : m_wifiManager(wifiManager), m_ntpClient(ntpClient), m_apiClient(apiClient), m_sensorManager(sensorManager) {}

namespace {
  class StatusPrinter {
  public:
    StatusPrinter(AsyncWebSocketClient* c) : m_client(c), m_offset(0) { memset(m_buf, 0, sizeof(m_buf)); }
    
    void print(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
      va_list args;
      va_start(args, fmt);
      int needed = vsnprintf(nullptr, 0, fmt, args);
      va_end(args);
      if (needed < 0) return;
      if (m_offset + needed >= sizeof(m_buf)) flush();
      va_start(args, fmt);
      m_offset += vsnprintf(m_buf + m_offset, sizeof(m_buf) - m_offset, fmt, args);
      va_end(args);
    }
    
    void flush() { 
      if (m_offset > 0) { 
        Utils::ws_send_encrypted(m_client, m_buf); 
        m_offset = 0; 
        volatile char* p = m_buf;
        size_t n = sizeof(m_buf);
        while(n--) *p++ = 0;
      }
    }
  private:
    AsyncWebSocketClient* m_client;
    char m_buf[512];
    size_t m_offset;
  };
}

void StatusCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  StatusPrinter p(context.client);
  char uptime[32], ntpSince[32], apiSince[32], timeStr[32] = "Not Synced";

  TerminalFormat::formatUptime(uptime, sizeof(uptime), millis());
  TerminalFormat::formatTimeSince(ntpSince, sizeof(ntpSince), m_ntpClient.getLastSyncMillis());
  TerminalFormat::formatTimeSince(apiSince, sizeof(apiSince), m_apiClient.getLastSuccessMillis());

  if (m_ntpClient.isTimeSynced()) {
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &ti);
  }

  p.print("\n==== Node Status ====\n");
  p.print("FW: %s | Up: %s\n", FIRMWARE_VERSION, uptime);
  p.print("Mem: %u | MaxBlk: %u | Frag: %d%%\n", ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(), ESP.getHeapFragmentation());

  WifiManager::State ws = m_wifiManager.getState();
  p.print("\n[WiFi] %s\n", ws == WifiManager::State::CONNECTED_STA ? "Connected" : 
                          (ws == WifiManager::State::PORTAL_MODE ? "Portal" : "Disconnected"));
  if (ws == WifiManager::State::CONNECTED_STA) {
    p.print("  SSID: %s (%d dBm)\n  IP: %s\n", WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str());
  } else if (ws == WifiManager::State::PORTAL_MODE) {
    p.print("  AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  }

  p.print("\n[Time] %s (sync: %s ago)\n", timeStr, ntpSince);
  p.print("[API] Last: %s ago\n", apiSince);
  p.print("[Sensors] SHT: %s | BH1750: %s\n", 
          m_sensorManager.getShtStatus() ? "OK" : "FAIL",
          m_sensorManager.getBh1750Status() ? "OK" : "FAIL");
  p.print("=====================\n");
  p.flush();
}
