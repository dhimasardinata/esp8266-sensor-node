#ifndef STATUS_COMMAND_H
#define STATUS_COMMAND_H

#include "ICommand.h"

// Forward declare dependencies
class WifiManager;
class NtpClient;
class ApiClient;
class ISensorManager;  // <-- Use the interface

class StatusCommand : public ICommand {
public:
  // --- MODIFICATION: Depend on the interface ---
  StatusCommand(WifiManager& wifiManager, NtpClient& ntpClient, ApiClient& apiClient, ISensorManager& sensorManager);

  const char* getName() const override {
    return "status";
  }
  const char* getDescription() const override {
    return "Displays current node status (WiFi, NTP, etc.).";
  }
  bool requiresAuth() const override {
    return false;
  }

  void execute(const CommandContext& context) override;

private:
  WifiManager& m_wifiManager;
  NtpClient& m_ntpClient;
  ApiClient& m_apiClient;
  // --- MODIFICATION: Store a reference to the interface ---
  ISensorManager& m_sensorManager;
};

#endif  // STATUS_COMMAND_H