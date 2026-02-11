#ifndef APPLICATION_H
#define APPLICATION_H

#include <IConfigObserver.h>
#include <IntervalTimer.h>

// Forward declarations
class ConfigManager;
class WifiManager;
class NtpClient;
class SensorManager;
class ApiClient;
class OtaManager;
class AppServer;
class PortalServer;
class DiagnosticsTerminal;

struct ApplicationServices {
  ConfigManager& configManager;
  WifiManager& wifiManager;
  NtpClient& ntpClient;
  SensorManager& sensorManager;
  ApiClient& apiClient;
  OtaManager& otaManager;
  AppServer& appServer;
  PortalServer& portalServer;

  DiagnosticsTerminal* terminal = nullptr;

  void setTerminal(DiagnosticsTerminal* term) {
    terminal = term;
  }
};

class Application : public IConfigObserver {
public:
  enum class State { INITIALIZING, SENSOR_STABILIZATION, CONNECTING, RUNNING, UPDATING, FLASHING_FIRMWARE };

  explicit Application(ApplicationServices& services) noexcept;

  void init();
  void loop();
  void onConfigUpdated() override;

private:
  void setState(State newState);
  void applyConfigs();

  void handleInitializing();
  void handleSensorStabilization();
  void handleConnecting();
  void handleRunning();
  void handleUpdating();
  void handleFlashing();

  ApplicationServices& m_services;

  State m_state;
  IntervalTimer m_stateTimer;
  IntervalTimer m_loopWdTimer;
  IntervalTimer m_healthCheckTimer{60000};  // Run health checks every 60 seconds.
  IntervalTimer m_otaTimer{100};            // Throttle ArduinoOTA.handle() to reduce CPU load.
  bool m_safeModeCleared = false;
  unsigned long m_bootTime = 0;
};

#endif
