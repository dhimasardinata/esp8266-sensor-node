#ifndef APPLICATION_H
#define APPLICATION_H

#include <IConfigObserver.h>
#include <IntervalTimer.h>

// Forward declarations
class DiagnosticsTerminal;
struct ServiceContainer;

class Application : public IConfigObserver {
public:
  enum class State { INITIALIZING, SENSOR_STABILIZATION, CONNECTING, RUNNING, UPDATING, FLASHING_FIRMWARE };

  explicit Application(ServiceContainer& services) noexcept;

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

  ServiceContainer& m_services;

  State m_state;
  IntervalTimer m_stateTimer;
  IntervalTimer m_loopWdTimer;
  
  // Factory Reset Button Logic
  void checkFactoryResetButton();
  unsigned long m_factoryResetPressStart = 0;
  bool m_isFactoryResetPressed = false;
};

#endif