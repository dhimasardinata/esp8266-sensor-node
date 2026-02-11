#include "Application.h"

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <Updater.h>

#include "ApiClient.h"
#include "AppServer.h"
#include "BootGuard.h"
#include "DiagnosticsTerminal.h"
#include "Logger.h"
#include "NtpClient.h"
#include "REDACTED"
#include "PortalServer.h"
#include "SensorManager.h"
#include "SystemHealth.h"
#include "constants.h"
#include "node_config.h"

Application::Application(ApplicationServices& services) noexcept
    : m_services(services),
      m_state(State::INITIALIZING),
      m_stateTimer(0),
      m_loopWdTimer(AppConstants::LOOP_WDT_TIMEOUT_MS) {}

void Application::init() {
  m_bootTime = millis();
  m_safeModeCleared = false;
  ESP.wdtEnable(8000);

  // Guard against boot loops caused by repeated crashes.
  if (BootGuard::getCrashCount() > 5) {
    LOG_ERROR("BOOT", F("CRITICAL: Boot loop detected (>5 crashes). Entering SAFE MODE (Portal Only)."));
    m_services.wifiManager.startPortal();  // Force Portal Mode
    setState(State::RUNNING);              // Bypass initialization; wifiManager.handle() manages the portal.
    return;
  }

  handleInitializing();
}

void Application::setState(State newState) {
  if (m_state == newState)
    return;
  m_state = newState;

  switch (m_state) {
    case State::SENSOR_STABILIZATION:
      LOG_INFO("APP", F("Waiting for sensors to stabilize..."));
      m_stateTimer.setInterval(AppConstants::SENSOR_STABILIZATION_DELAY_MS);
      m_stateTimer.reset();
      break;
    case State::FLASHING_FIRMWARE:
      LOG_INFO("FLASH", F("Starting flash from LittleFS..."));
      handleFlashing();
      break;
    case State::RUNNING:
      LOG_INFO("APP", F("Firmware Version: %s"), FIRMWARE_VERSION);
      LOG_INFO("APP", F("GH_ID: %d, NODE_ID: %d"), GH_ID, NODE_ID);
      LOG_INFO("APP", F("Setup complete. Starting main loop..."));
      break;
    case State::INITIALIZING:
    case State::CONNECTING:
    case State::UPDATING:
      break;
    default:
      break;
  }
}

void Application::loop() {
  // FIX: Check WDT BEFORE reset to detect cumulative slow loops
  if (m_loopWdTimer.hasElapsed(false)) {
    LOG_ERROR("APP", F("CRITICAL: Loop WDT triggered. Rebooting!"));
    BootGuard::setRebootReason(BootGuard::RebootReason::SOFT_WDT);
    delay(AppConstants::REBOOT_DELAY_MS);
    ESP.restart();
  }

  // Reset timer for this iteration
  m_loopWdTimer.reset();
  ESP.wdtFeed();

  // Maintain essential services.
  m_services.portalServer.handle();

  switch (m_state) {
    case State::INITIALIZING:
      break;
    case State::SENSOR_STABILIZATION:
      handleSensorStabilization();
      break;
    case State::CONNECTING:
      handleConnecting();
      break;
    case State::RUNNING:
      handleRunning();
      break;
    case State::UPDATING:
      handleUpdating();
      break;
    case State::FLASHING_FIRMWARE:
      break;
    default:
      LOG_ERROR("APP", F("Unknown application state!"));
      break;
  }

  yield();
}

void Application::handleInitializing() {
  ArduinoOTA.onStart([this]() { setState(State:REDACTED
  ArduinoOTA.onEnd([this]() { setState(State:REDACTED
  ArduinoOTA.onError([this](ota_error_t) { setState(State:REDACTED
  m_services.appServer.onFlashRequest([this]() { setState(State::FLASHING_FIRMWARE); });

  applyConfigs();
  setState(State::SENSOR_STABILIZATION);
}

void Application::handleSensorStabilization() {
  if (m_stateTimer.hasElapsed()) {
    setState(State::CONNECTING);
  }
}

void Application::handleConnecting() {
  m_services.wifiManager.handle();
  if (m_services.wifiManager.getState() =REDACTED
    setState(State::RUNNING);
  }
}

void Application::handleRunning() {
  auto& health = SystemHealth::HealthMonitor::instance();
  health.recordLoopTick();

  m_services.wifiManager.handle();
  m_services.ntpClient.handle();
  m_services.sensorManager.handle();

  // Never run OTA while an upload is active
  m_services.otaManager.setUploadInProgress(m_services.apiClient.isUploadActive());
  m_services.otaManager.handle();

  // Defer uploads while OTA is busy (if OTA handle becomes async in the future)
  m_services.apiClient.setOtaInProgress(m_services.otaManager.isBusy());
  m_services.apiClient.handle();
  m_services.appServer.handle();

  // Ensure the terminal session remains active.
  if (m_services.terminal) {
    m_services.terminal->handle();
  }

  if (m_otaTimer.hasElapsed()) {
    ArduinoOTA.handle();
  }

  // FIX: Safe Mode Auto-Recovery - Clear crash counter after stable portal uptime
  if (BootGuard::getCrashCount() > 5 && !m_safeModeCleared && millis() > 300000) {
    LOG_INFO("BOOT", F("Safe mode stable for 5min, clearing crash counter"));
    BootGuard::clear();
    m_safeModeCleared = true;
  }

  // Monitor system health metrics every minute.
  if (m_healthCheckTimer.hasElapsed()) {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    int32_t rssi = WiFi.isConnected() ? WiFi.RSSI() : 0;
    bool shtOk = m_services.sensorManager.getShtStatus();
    bool bh1750Ok = m_services.sensorManager.getBh1750Status();

    auto score = health.calculateHealth(freeHeap, maxBlock, rssi, shtOk, bh1750Ok);

    // Log health status
    if (score.overall() < 25) {
      LOG_WARN("HEALTH",
               F("Score: %u/100 (%s) - Heap:%u Frag:%u CPU:%u WiFi:%u Sensor:%u"),
               score.overall(),
               score.getGrade(),
               score.heap,
               score.fragmentation,
               score.cpu,
               score.wifi,
               score.sensor);
    } else if (score.overall() < 50) {
      LOG_INFO("HEALTH", F("Score: %u/100 (%s)"), score.overall(), score.getGrade());
    }

    // FIX: Improved uptime calculation with overflow handling
    const unsigned long bootTime = m_bootTime;
    unsigned long uptimeMs = millis();

    // Handle millis() overflow (happens after ~49 days)
    if (uptimeMs < bootTime) {
      // Overflow occurred - calculate wrapped uptime
      uptimeMs = (0xFFFFFFFF - bootTime) + uptimeMs;
    } else {
      uptimeMs = uptimeMs - bootTime;
    }

    unsigned long uptimeHours = uptimeMs / 3600000UL;

    // Proactively schedule a reboot if health is critical.
    if (score.needsReboot() && !health.isRebootScheduled()) {
      if (uptimeHours >= 1) {  // Only if running for at least 1 hour
        LOG_WARN("HEALTH", F("Critical health. Scheduling maintenance reboot in 60s."));
        health.scheduleReboot();
      }
    }

    // Monitor execution loop duration.
    const auto& metrics = health.getLoopMetrics();
    if (metrics.getSlowLoopPercent() > 5) {
      LOG_WARN("CPU", F("Slow loops: %u%% (max: %lu us)"), metrics.getSlowLoopPercent(), metrics.maxDurationUs);
    }

    // Prevent metric overflow.
    health.periodicReset();
  }

  // Execute scheduled maintenance reboots.
  if (health.shouldRebootNow()) {
    LOG_WARN("HEALTH", F("Maintenance reboot triggered."));
    BootGuard::setRebootReason(BootGuard::RebootReason::HEALTH_CHECK);
    delay(100);
    ESP.restart();
  }

}

void Application::handleUpdating() {
  ArduinoOTA.handle();
}

void Application::handleFlashing() {
  File binFile = LittleFS.open("/update.bin", "r");
  if (!binFile) {
    LOG_ERROR("FLASH", F("ERROR: Could not open bin file. Aborting."));
    setState(State::RUNNING);
    return;
  }

  if (binFile.size() == 0) {
    LOG_ERROR("FLASH", F("ERROR: File is empty (0 bytes). Aborting."));
    binFile.close();
    LittleFS.remove("/update.bin");
    setState(State::RUNNING);
    return;
  }

  size_t actualSize = binFile.size();

  // FIX: Disable WDT during flash operation (can take 10-30 seconds)
  ESP.wdtDisable();

  if (!Update.begin(actualSize, U_FLASH)) {
    LOG_ERROR("FLASH", F("ERROR: Not enough space. Error: %d"), Update.getError());

    // FIX: Clean up and return on error - don't continue to writeStream!
    binFile.close();
    LittleFS.remove("/update.bin");
    ESP.wdtEnable(8000);  // Re-enable WDT
    setState(State::RUNNING);
    return;
  }

  // Now safe to proceed with flash
  size_t written = Update.writeStream(binFile);

  // Re-enable WDT after flash operation
  ESP.wdtEnable(8000);

  if (Update.end(true)) {
    LOG_INFO("FLASH", F("SUCCESS! Written %u bytes. Rebooting..."), written);
    BootGuard::setRebootReason(BootGuard::RebootReason::OTA_UPDATE);
    binFile.close();
    LittleFS.remove("/update.bin");
    delay(1000);
    ESP.restart();
  } else {
    LOG_ERROR("FLASH", F("ERROR: Finalizing update failed. Error: %d"), Update.getError());
    binFile.close();
    LittleFS.remove("/update.bin");
    setState(State::RUNNING);
  }
}

void Application::onConfigUpdated() {
  applyConfigs();
}

void Application::applyConfigs() {
  LOG_INFO("CONFIG", F("Applying configuration to all modules..."));
  m_services.apiClient.applyConfig(m_services.configManager.getConfig());
  m_services.otaManager.applyConfig(m_services.configManager.getConfig());
}
