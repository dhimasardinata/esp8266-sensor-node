#include "SysInfoCommand.h"

#include <ESP8266WiFi.h>

#include "node_config.h"
#include "utils.h"

namespace {
  const char* getFlashMode(FlashMode_t mode) {
    switch (mode) {
      case FM_QIO:  return "QIO";
      case FM_QOUT: return "QOUT";
      case FM_DIO:  return "DIO";
      case FM_DOUT: return "DOUT";
      default:      return "?";
    }
  }
}

void SysInfoCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend()) return;

  Utils::ws_printf(context.client,
    "\n--- System Info ---\n"
    "Node: GH%d-N%d | FW: %s\n"
    "[Chip] ID: 0x%06X | CPU: %uMHz | SDK: %s\n"
    "[Flash] ID: 0x%06X | %uKB @ %uMHz (%s)\n"
    "[FW] %uKB / %uKB free | MAC: %s\n"
    "-------------------\n",
    GH_ID, NODE_ID, FIRMWARE_VERSION,
    ESP.getChipId(), ESP.getCpuFreqMHz(), ESP.getSdkVersion(),
    ESP.getFlashChipId(), ESP.getFlashChipSize() / 1024, 
    ESP.getFlashChipSpeed() / 1000000, getFlashMode(ESP.getFlashChipMode()),
    ESP.getSketchSize() / 1024, ESP.getFreeSketchSpace() / 1024, 
    WiFi.macAddress().c_str());
}
