#include "CheckUpdateCommand.h"

#include "NtpClient.h"
#include "REDACTED"
#include "REDACTED"
#include "utils.h"

CheckUpdateCommand::CheckUpdateCommand(OtaManager& otaManager, WifiManager& wifiManager, NtpClient& ntpClient)
    : m_otaManager(otaManager), m_wifiManager(wifiManager), m_ntpClient(ntpClient) {}

void CheckUpdateCommand::execute(const CommandContext& context) {
  if (m_wifiManager.getState() != REDACTED
    Utils::ws_printf(context.client, "[ERROR] Cannot check for updates, WiFi is not connected.");
  } else if (!m_ntpClient.isTimeSynced()) {
    Utils::ws_printf(context.client, "[ERROR] Cannot check for updates, time is not synced. Please wait.");
  } else {
    Utils::ws_printf(context.client, "Update check scheduled. See serial monitor for details.");
    m_otaManager.forceUpdateCheck();
  }
}