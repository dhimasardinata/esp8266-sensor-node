#include "SysInfoCommand.h"

#include <ESP8266WiFi.h>

#include "generated/node_config.h"
#include "support/Utils.h"

namespace {
  PGM_P getFlashMode(FlashMode_t mode) {
    switch (mode) {
      case FM_QIO:  return PSTR("QIO");
      case FM_QOUT: return PSTR("QOUT");
      case FM_DIO:  return PSTR("DIO");
      case FM_DOUT: return PSTR("DOUT");
      default:      return PSTR("?");
    }
  }
}

void SysInfoCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;
  char flashMode[8];
  strncpy_P(flashMode, getFlashMode(ESP.getFlashChipMode()), sizeof(flashMode) - 1);
  flashMode[sizeof(flashMode) - 1] = '\0';
  const String mac = WiFi.macAddress();

  Utils::ws_printf_P(context.client, PSTR("\n--- System Info ---\n"));
  Utils::ws_printf_P(context.client, PSTR("Node: GH%d-N%d | FW: %s\n"), GH_ID, NODE_ID, FIRMWARE_VERSION);
  Utils::ws_printf_P(context.client,
                     PSTR("[Chip] ID: 0x%06X | CPU: %uMHz | SDK: %s\n"),
                     ESP.getChipId(),
                     ESP.getCpuFreqMHz(),
                     ESP.getSdkVersion());
  Utils::ws_printf_P(context.client,
                     PSTR("[Flash] ID: 0x%06X | %uKB @ %uMHz (%s)\n"),
                     ESP.getFlashChipId(),
                     ESP.getFlashChipSize() / 1024,
                     ESP.getFlashChipSpeed() / 1000000,
                     flashMode);
  Utils::ws_printf_P(context.client,
                     PSTR("[FW] %uKB / %uKB free | MAC: %s\n"),
                     ESP.getSketchSize() / 1024,
                     ESP.getFreeSketchSpace() / 1024,
                     mac.c_str());
  Utils::ws_printf_P(context.client, PSTR("-------------------\n"));
}
