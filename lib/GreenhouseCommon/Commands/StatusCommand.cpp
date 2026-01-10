#include "StatusCommand.h"

#include <ESP8266WiFi.h>
#include <stdarg.h>

#include "ApiClient.h"
#include "ISensorManager.h"
#include "NtpClient.h"
#include "TerminalFormatting.h"
#include "WifiManager.h"
#include "utils.h"

StatusCommand::StatusCommand(WifiManager& wifiManager,
                             NtpClient& ntpClient,
                             ApiClient& apiClient,
                             ISensorManager& sensorManager)
    : m_wifiManager(wifiManager), m_ntpClient(ntpClient), m_apiClient(apiClient), m_sensorManager(sensorManager) {}

void StatusCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend())
    return;

  // === BUFFERED OUTPUT (fixes truncation issue) ===
  char buffer[512];
  size_t offset = 0;

  // Lambda for smart buffered append (same pattern as HelpCommand)
  auto append = [&](const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    va_list args;
    
    // Calculate needed size
    va_start(args, fmt);
    int needed = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);
    
    if (needed < 0) return;
    
    // Flush if buffer would overflow
    if (offset + needed >= sizeof(buffer)) {
      Utils::ws_send_encrypted(context.client, buffer);
      offset = 0;
      memset(buffer, 0, sizeof(buffer));
    }
    
    // Write to buffer
    va_start(args, fmt);
    offset += vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);
    va_end(args);
  };

  // Gather data BEFORE output (avoid interleaved calls)
  char timeBuffer[32] = {0};
  char uptimeBuffer[32] = {0};
  char ntpSinceBuffer[32] = {0};
  char apiSinceBuffer[32] = {0};

  TerminalFormat::formatUptime(uptimeBuffer, sizeof(uptimeBuffer), millis());
  TerminalFormat::formatTimeSince(ntpSinceBuffer, sizeof(ntpSinceBuffer), m_ntpClient.getLastSyncMillis());
  TerminalFormat::formatTimeSince(apiSinceBuffer, sizeof(apiSinceBuffer), m_apiClient.getLastSuccessMillis());

  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t maxBlock = ESP.getMaxFreeBlockSize();
  uint8_t frag = ESP.getHeapFragmentation();

  if (m_ntpClient.isTimeSynced()) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
  }

  WifiManager::State wifiState = m_wifiManager.getState();
  const char* wifiStatus = "Disconnected";
  String networkInfo;
  String ipInfo;
  
  if (wifiState == WifiManager::State::CONNECTED_STA) {
    wifiStatus = "Connected";
    networkInfo = String(WiFi.SSID()) + " (" + String(WiFi.RSSI()) + " dBm)";
    ipInfo = WiFi.localIP().toString();
  } else if (wifiState == WifiManager::State::PORTAL_MODE) {
    wifiStatus = "Portal Active";
    ipInfo = WiFi.softAPIP().toString();
  }

  // === BUILD OUTPUT IN BUFFER ===
  append("\n========================================\n");
  append("Node Status\n");
  append("========================================\n\n");
  
  append("FW: %s | Uptime: %s\n", FIRMWARE_VERSION, uptimeBuffer);
  append("Mem: %u Free | MaxBlk: %u | Frag: %d%%\n\n", freeHeap, maxBlock, frag);

  append("\n--- WiFi ---\n");
  append("  %-16s: %s\n", "Status", wifiStatus);
  if (networkInfo.length() > 0) {
    append("  %-16s: %s\n", "Network", networkInfo.c_str());
  }
  if (ipInfo.length() > 0) {
    append("  %-16s: %s\n", wifiState == WifiManager::State::PORTAL_MODE ? "AP IP" : "IP", ipInfo.c_str());
  }

  append("\n--- Time ---\n");
  append("  %-16s: %s\n", "Current", m_ntpClient.isTimeSynced() ? timeBuffer : "Not Synced");
  append("  %-16s: %s ago\n", "Last Sync", ntpSinceBuffer);

  append("\n--- API ---\n");
  append("  %-16s: %s ago\n", "Last Upload", apiSinceBuffer);

  append("\n--- Sensors ---\n");
  append("  %-16s: %s\n", "SHT", m_sensorManager.getShtStatus() ? "OK" : "FAIL");
  append("  %-16s: %s\n", "BH1750", m_sensorManager.getBh1750Status() ? "OK" : "FAIL");

  append("----------------------------------------\n");

  // Send remaining buffer
  if (offset > 0) {
    Utils::ws_send_encrypted(context.client, buffer);
  }
}
