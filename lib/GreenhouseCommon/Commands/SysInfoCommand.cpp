#include "SysInfoCommand.h"

#include <ESP8266WiFi.h>

#include "node_config.h"
#include "utils.h"

void SysInfoCommand::execute(const CommandContext& context) {
  if (!context.client || !context.client->canSend())
    return;

  // Gather system information
  uint32_t chipId = ESP.getChipId();
  uint32_t flashId = ESP.getFlashChipId();
  uint32_t flashSize = ESP.getFlashChipSize();
  uint32_t flashSpeed = ESP.getFlashChipSpeed();
  FlashMode_t flashMode = ESP.getFlashChipMode();
  uint32_t sketchSize = ESP.getSketchSize();
  uint32_t freeSketch = ESP.getFreeSketchSpace();
  uint8_t cpuFreq = ESP.getCpuFreqMHz();
  
  const char* flashModeStr = "Unknown";
  switch (flashMode) {
    case FM_QIO:  flashModeStr = "QIO"; break;
    case FM_QOUT: flashModeStr = "QOUT"; break;
    case FM_DIO:  flashModeStr = "DIO"; break;
    case FM_DOUT: flashModeStr = "DOUT"; break;
    default: break;
  }

  Utils::ws_printf(context.client,
    "\n--- System Info ---\n"
    "Node: GH%d-N%d | FW: %s\n"
    "-------------------\n"
    "[Chip]\n"
    "  ID: 0x%06X\n"
    "  CPU: %u MHz\n"
    "  SDK: %s\n"
    "[Flash]\n"
    "  ID: 0x%06X\n"
    "  Size: %u KB\n"
    "  Speed: %u MHz\n"
    "  Mode: %s\n"
    "[Firmware]\n"
    "  Sketch: %u KB / %u KB free\n"
    "  MAC: %s\n"
    "-------------------\n",
    GH_ID, NODE_ID, FIRMWARE_VERSION,
    chipId,
    cpuFreq,
    ESP.getSdkVersion(),
    flashId,
    flashSize / 1024,
    flashSpeed / 1000000,
    flashModeStr,
    sketchSize / 1024, freeSketch / 1024,
    WiFi.macAddress().c_str()
  );
}
