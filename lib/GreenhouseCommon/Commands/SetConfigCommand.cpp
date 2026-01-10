#include "SetConfigCommand.h"

#include "ConfigManager.h"
#include "constants.h"
#include "utils.h"

SetConfigCommand::SetConfigCommand(ConfigManager& configManager) : m_configManager(configManager) {}

void SetConfigCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) {
    return;
  }

  // Buffers for key and value
  char key_buffer[32];
  char value_buffer[32];

  // Parse using sscanf
  // %31s reads a string up to 31 chars (leaving 1 for null) until a whitespace
  if (sscanf(context.args, "%31s %31s", key_buffer, value_buffer) != 2) {
    Utils::ws_printf(context.client,
                     "[ERROR] Invalid format. Usage: setconfig <key> <value>\n"
                     "Available keys: upload_interval, sample_interval, "
                     "cache_interval, sw_wdt_timeout");
    return;
  }

  unsigned long value = atol(value_buffer);
  
  // HARDENING: Input bounds validation
  if (value < AppConstants::INTERVAL_MIN_MS || value > AppConstants::INTERVAL_MAX_MS) {
    Utils::ws_printf(context.client,
                     "[ERROR] Value out of range. Must be between %lu and %lu ms.\n",
                     AppConstants::INTERVAL_MIN_MS,
                     AppConstants::INTERVAL_MAX_MS);
    return;
  }
  
  AppConfig tempConfig = m_configManager.getConfig();
  const AppConfig& originalConfig = m_configManager.getConfig();
  bool key_found = true;

  if (strcasecmp(key_buffer, "upload_interval") == 0) {
    tempConfig.DATA_UPLOAD_INTERVAL_MS = value;
  } else if (strcasecmp(key_buffer, "sample_interval") == 0) {
    tempConfig.SENSOR_SAMPLE_INTERVAL_MS = value;
  } else if (strcasecmp(key_buffer, "cache_interval") == 0) {
    tempConfig.CACHE_SEND_INTERVAL_MS = value;
  } else if (strcasecmp(key_buffer, "sw_wdt_timeout") == 0) {
    tempConfig.SOFTWARE_WDT_TIMEOUT_MS = value;
  } else {
    Utils::ws_printf(context.client, "[ERROR] Unknown configuration key: '%s'\n", key_buffer);
    key_found = false;
  }

  if (key_found) {
    m_configManager.setTimingConfig(tempConfig);
    const AppConfig& newConfig = m_configManager.getConfig();

    ConfigStatus saveStatus = m_configManager.save();

    Utils::ws_printf(context.client,
                     "[SUCCESS] Configuration '%s' set to '%lu'.\n"
                     "%s%s%s%s",
                     key_buffer,
                     value,
                     (newConfig.DATA_UPLOAD_INTERVAL_MS != originalConfig.DATA_UPLOAD_INTERVAL_MS &&
                      strcasecmp(key_buffer, "upload_interval") != 0)
                         ? "[NOTE] `upload_interval` was auto-adjusted for consistency.\n"
                         : "",
                     (newConfig.SOFTWARE_WDT_TIMEOUT_MS != originalConfig.SOFTWARE_WDT_TIMEOUT_MS &&
                      strcasecmp(key_buffer, "sw_wdt_timeout") != 0)
                         ? "[NOTE] `sw_wdt_timeout` was auto-adjusted for consistency.\n"
                         : "",
                     (saveStatus == ConfigStatus::OK) ? "Configuration saved to file.\n"
                                                      : "[CRITICAL] Failed to save configuration to file!\n",
                     (saveStatus == ConfigStatus::OK) ? "[SUCCESS] Settings are being applied live.\n" : "");
  }
}