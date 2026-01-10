#include "CrashHandler.h"

#include <LittleFS.h>
#include <user_interface.h>  // For struct rst_info

#include "node_config.h"  // For FIRMWARE_VERSION
#include "Paths.h"

void CrashHandler::process() {
  struct rst_info* rst = system_get_rst_info();

  // Filter out normal startups (Power On, Hardware Reset, or Intentional Deep Sleep)
  if (!rst || rst->reason == REASON_DEFAULT_RST || rst->reason == REASON_EXT_SYS_RST ||
      rst->reason == REASON_DEEP_SLEEP_AWAKE) {
    return;
  }

  // Safety: Check filesystem space First!
  FSInfo fs_info;
  if (LittleFS.info(fs_info)) {
      // If less than 2KB free, delete the crash log to make room
      if (fs_info.totalBytes - fs_info.usedBytes < 2048) {
           LittleFS.remove(Paths::CRASH_LOG);
      }
  }

  // Safety: Prevent unlimited growth
  {
    File f_check = LittleFS.open(Paths::CRASH_LOG, "r");
    if (f_check) {
      if (f_check.size() > 2048) {
        f_check.close();
        LittleFS.remove(Paths::CRASH_LOG);
      } else {
        f_check.close();
      }
    }
  }

  // Open file in append mode to keep history (limit size manually if needed)
  File f = LittleFS.open(Paths::CRASH_LOG, "a");
  if (!f)
    return;

  // Timestamp header (relative to boot, obviously 0 here, but useful delimiter)
  f.println("\n--- CRASH REPORT ---");
  f.printf("Firmware: %s (GH%d-N%d)\n", FIRMWARE_VERSION, GH_ID, NODE_ID);

  switch (rst->reason) {
    case REASON_WDT_RST:
      f.println("Reason: Hardware Watchdog Reset");
      break;
    case REASON_EXCEPTION_RST:
      f.println("Reason: Fatal Exception");
      break;
    case REASON_SOFT_WDT_RST:
      f.println("Reason: Software Watchdog Reset");
      break;
    case REASON_SOFT_RESTART:
      f.println("Reason: Software Restart (ESP.restart)");
      break;
    default:
      f.printf("Reason: Unknown (%d)\n", rst->reason);
      break;
  }

  if (rst->reason == REASON_EXCEPTION_RST) {
    f.printf("Exception: %d\n", rst->exccause);
    f.printf("EPC1: 0x%08x\n", rst->epc1);
    f.printf("EPC2: 0x%08x\n", rst->epc2);
    f.printf("EPC3: 0x%08x\n", rst->epc3);
    f.printf("EXCVADDR: 0x%08x\n", rst->excvaddr);
    f.printf("DEPC: 0x%08x\n", rst->depc);
  }

  f.println("--------------------");
  f.close();
}

String CrashHandler::getLog() {
  if (!LittleFS.exists(Paths::CRASH_LOG))
    return "No crash logs found.";
  File f = LittleFS.open(Paths::CRASH_LOG, "r");
  if (!f)
    return "Failed to open crash log.";
  String s = f.readString();
  f.close();
  return s;
}

void CrashHandler::clearLog() {
  LittleFS.remove(Paths::CRASH_LOG);
}

bool CrashHandler::hasCrashLog() {
  return LittleFS.exists(Paths::CRASH_LOG);
}