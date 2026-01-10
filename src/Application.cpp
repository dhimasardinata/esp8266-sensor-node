#include "Application.h"

#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <Updater.h>

#include "ApiClient.h"
#include "AppServer.h"
#include "BootGuard.h"
#include "DiagnosticsTerminal.h"
#include "ICacheManager.h"
#include "NtpClient.h"
#include "OtaManager.h"
#include "PortalServer.h"
#include "SensorManager.h"
#include "ServiceContainer.h"
#include "constants.h"
#include "node_config.h"
#include "Logger.h"

Application::Application(ServiceContainer& services) noexcept
    : m_services(services),
      m_state(State::INITIALIZING),
      m_stateTimer(0),
      m_loopWdTimer(AppConstants::LOOP_WDT_TIMEOUT_MS) {}

void Application::init() {
  ESP.wdtEnable(8000);
  pinMode(0, INPUT_PULLUP);  // Factory Reset Button (GPIO 0)

  // Check for bootloop (Crash Loop Protection)
  if (BootGuard::getCrashCount() > 5) {
    LOG_ERROR("BOOT", F("CRITICAL: Boot loop detected (>5 crashes). Entering SAFE MODE (Portal Only)."));
    m_services.wifiManager.startPortal();  // Force Portal Mode
    setState(State::RUNNING);  // Skip initialization, go effectively to a limited running state (Portal is handled by
                               // wifiManager.handle())
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
  ESP.wdtFeed();
  m_loopWdTimer.reset();

  // Service inti yang harus selalu berjalan
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

  if (m_loopWdTimer.hasElapsed(false)) {
    LOG_ERROR("APP", F("CRITICAL: Loop WDT triggered. Rebooting!"));
    delay(AppConstants::REBOOT_DELAY_MS);
    ESP.restart();
  }

  yield();
  checkFactoryResetButton();
}

void Application::checkFactoryResetButton() {
  // GPIO 0 is usually the FLASH button on ESP8266 boards
  constexpr uint8_t FACTORY_RESET_PIN = 0;

  if (digitalRead(FACTORY_RESET_PIN) == LOW) {
    if (!m_isFactoryResetPressed) {
      m_isFactoryResetPressed = true;
      m_factoryResetPressStart = millis();
      LOG_INFO("RESET", F("Button Pressed. Hold 5s to Factory Reset..."));
    } else {
      if (millis() - m_factoryResetPressStart > 5000) {
        LOG_WARN("RESET", F("FACTORY RESET TRIGGERED BY BUTTON!"));
        // Visual feedback if possible (e.g., LED blink fast)
        if (!m_services.configManager.factoryReset()) {
          LOG_ERROR("RESET", F("Warning: Factory reset encountered an error."));
        }
        m_services.cacheManager.reset();
        BootGuard::clear();
        LOG_INFO("RESET", F("Rebooting..."));
        delay(1000);
        ESP.restart();
      }
    }
  } else {
    if (m_isFactoryResetPressed) {
      m_isFactoryResetPressed = false;
      if (millis() - m_factoryResetPressStart < 5000) {
        LOG_INFO("RESET", F("Button released. Cancelled."));
      }
    }
    // Prevent immediate re-trigger if millis wraps? Unlikely.
  }
}

void Application::handleInitializing() {
  ArduinoOTA.onStart([this]() { setState(State::UPDATING); });
  ArduinoOTA.onEnd([this]() { setState(State::RUNNING); });
  ArduinoOTA.onError([this](ota_error_t) { setState(State::RUNNING); });
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
  if (m_services.wifiManager.getState() == WifiManager::State::CONNECTED_STA) {
    setState(State::RUNNING);
  }
}

void Application::handleRunning() {
  m_services.wifiManager.handle();
  m_services.ntpClient.handle();
  m_services.sensorManager.handle();
  m_services.apiClient.handle();
  m_services.otaManager.handle();
  m_services.appServer.handle();

  ArduinoOTA.handle();

  // TOOLING: Memory monitoring - check heap health periodically
  static unsigned long lastMemCheck = 0;
  if (millis() - lastMemCheck > 60000) {  // Check every minute
    lastMemCheck = millis();
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    uint8_t fragmentation = static_cast<uint8_t>(100 - (maxBlock * 100 / freeHeap));
    
    if (freeHeap < AppConstants::HEAP_CRITICAL_THRESHOLD) {
      LOG_WARN("MEM-CRIT", F("Free: %u bytes! Reboot recommended."), freeHeap);
    } else if (freeHeap < AppConstants::HEAP_WARNING_THRESHOLD) {
      LOG_WARN("MEM-WARN", F("Free: %u bytes, Frag: %u%%"), freeHeap, fragmentation);
    }
    
    if (fragmentation > AppConstants::FRAGMENTATION_WARNING_PERCENT) {
      LOG_WARN("MEM-WARN", F("High fragmentation: %u%%"), fragmentation);
    }
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
  if (!Update.begin(actualSize, U_FLASH)) {
    LOG_ERROR("FLASH", F("ERROR: Not enough space. Error: %d"), Update.getError());
  } else {
    Update.writeStream(binFile);
    if (Update.end(true)) {
      LOG_INFO("FLASH", F("SUCCESS! Rebooting..."));
      delay(1000);
      ESP.restart();
    } else {
      LOG_ERROR("FLASH", F("ERROR: Finalizing update failed. Error: %d"), Update.getError());
    }
  }
  binFile.close();
  LittleFS.remove("/update.bin");
  setState(State::RUNNING);
}

void Application::onConfigUpdated() {
  applyConfigs();
}

void Application::applyConfigs() {
  LOG_INFO("CONFIG", F("Applying configuration to all modules..."));
  m_services.apiClient.applyConfig(m_services.configManager.getConfig());
  m_services.otaManager.applyConfig(m_services.configManager.getConfig());
}