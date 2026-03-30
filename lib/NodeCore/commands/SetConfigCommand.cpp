#include "SetConfigCommand.h"

#include "system/ConfigManager.h"
#include "config/constants.h"
#include "support/Utils.h"

SetConfigCommand::SetConfigCommand(ConfigManager& configManager) : m_configManager(configManager) {}

namespace {
  enum class ConfigKey { UPLOAD, SAMPLE, CACHE, WDT, UNKNOWN };

  ConfigKey parseKey(const char* key) {
    if (strcasecmp_P(key, PSTR("upload_interval")) == 0) return ConfigKey::UPLOAD;
    if (strcasecmp_P(key, PSTR("sample_interval")) == 0) return ConfigKey::SAMPLE;
    if (strcasecmp_P(key, PSTR("cache_interval")) == 0) return ConfigKey::CACHE;
    if (strcasecmp_P(key, PSTR("sw_wdt_timeout")) == 0) return ConfigKey::WDT;
    return ConfigKey::UNKNOWN;
  }
}

void SetConfigCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  char key[32] = {0};
  char val[32] = {0};
  // Use consistent field widths (31 chars + null terminator = 32)
  if (sscanf(context.args, "%31s %31s", key, val) != 2) {
    Utils::ws_printf_P(context.client,
                       PSTR("[ERROR] Usage: setconfig <key> <value>\nKeys: upload_interval, sample_interval, cache_interval, sw_wdt_timeout"));
    return;
  }

  char* endptr;
  unsigned long value = strtoul(val, &endptr, 10);
  if (*endptr != '\0' || value < AppConstants::INTERVAL_MIN_MS || value > AppConstants::INTERVAL_MAX_MS) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Value must be %lu-%lu ms."),
                       AppConstants::INTERVAL_MIN_MS, AppConstants::INTERVAL_MAX_MS);
    return;
  }

  ConfigKey k = parseKey(key);
  if (k == ConfigKey::UNKNOWN) {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] Unknown key: '%s'"), key);
    return;
  }

  AppConfig cfg = m_configManager.getConfig();
  switch (k) {
    case ConfigKey::UPLOAD: cfg.DATA_UPLOAD_INTERVAL_MS = value; break;
    case ConfigKey::SAMPLE: cfg.SENSOR_SAMPLE_INTERVAL_MS = value; break;
    case ConfigKey::CACHE:  cfg.CACHE_SEND_INTERVAL_MS = value; break;
    case ConfigKey::WDT:    cfg.SOFTWARE_WDT_TIMEOUT_MS = value; break;
    default: break;
  }

  m_configManager.setTimingConfig(cfg);
  ConfigStatus status = m_configManager.save();
  m_configManager.releaseStrings();
  if (status == ConfigStatus::OK) {
    Utils::ws_printf_P(context.client, PSTR("[SUCCESS] '%s' set to %lu ms."), key, value);
  } else {
    Utils::ws_printf_P(context.client, PSTR("[ERROR] '%s' set to %lu ms."), key, value);
  }
}
