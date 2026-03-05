#include "CrashHandler.h"

#include <LittleFS.h>
#include <user_interface.h>  // For struct rst_info

#include "Paths.h"
#include "node_config.h"  // For FIRMWARE_VERSION

namespace {
  bool isNormalReset(rst_info* rst) {
    if (!rst)
      return true;
    return rst->reason == REASON_DEFAULT_RST || rst->reason == REASON_EXT_SYS_RST ||
           rst->reason == REASON_DEEP_SLEEP_AWAKE;
  }

  void ensureLogSpace() {
    FSInfo fs_info;
    if (LittleFS.info(fs_info) && (fs_info.totalBytes - fs_info.usedBytes < 2048)) {
      LittleFS.remove(Paths::CRASH_LOG);
    }
    File f = LittleFS.open(Paths::CRASH_LOG, "r");
    if (f && f.size() > 2048) {
      f.close();
      LittleFS.remove(Paths::CRASH_LOG);
    } else if (f) {
      f.close();
    }
  }

  const char* getResetReason(uint32_t reason) {
    switch (reason) {
      case REASON_WDT_RST:
        return "Hardware WDT";
      case REASON_EXCEPTION_RST:
        return "Fatal Exception";
      case REASON_SOFT_WDT_RST:
        return "Software WDT";
      case REASON_SOFT_RESTART:
        return "ESP.restart";
      default:
        return "Unknown";
    }
  }
}  // namespace

void CrashHandler::process() {
  struct rst_info* rst = system_get_rst_info();
  if (isNormalReset(rst))
    return;

  ensureLogSpace();

  File f = LittleFS.open(Paths::CRASH_LOG, "a");
  if (!f)
    return;

  f.println("\n--- CRASH REPORT ---");
  f.printf("FW: %s (GH%d-N%d)\n", FIRMWARE_VERSION, GH_ID, NODE_ID);
  f.printf("Reason: %s (%d)\n", getResetReason(rst->reason), rst->reason);

  if (rst->reason == REASON_EXCEPTION_RST) {
    f.printf("Ex:%d EPC1:0x%08x EPC2:0x%08x EPC3:0x%08x\n", rst->exccause, rst->epc1, rst->epc2, rst->epc3);
    f.printf("EXCV:0x%08x DEPC:0x%08x\n", rst->excvaddr, rst->depc);
  }

  f.println("--------------------");
  f.close();
}

// OOM Protection: Do NOT read the entire file if it's large.
String CrashHandler::getLog() {
  if (!LittleFS.exists(Paths::CRASH_LOG))
    return "No logs.";
  File f = LittleFS.open(Paths::CRASH_LOG, "r");
  if (!f)
    return "Error opening log.";

  // Safety: Only read the LAST 1024 bytes directly
  size_t size = f.size();
  if (size > 1024) {
    f.seek(size - 1024);
  }

  String s = f.readString();
  f.close();

  if (size > 1024)
    return "[...truncated...] " + s;
  return s;
}

size_t CrashHandler::streamLogTo(Print& output, size_t maxBytes) {
  if (!LittleFS.exists(Paths::CRASH_LOG)) {
    output.println(F("No crash logs found."));
    return 0;
  }
  File f = LittleFS.open(Paths::CRASH_LOG, "r");
  if (!f) {
    output.println(F("Failed to open crash log."));
    return 0;
  }

  size_t fileSize = f.size();
  size_t toRead = (maxBytes > 0 && maxBytes < fileSize) ? maxBytes : fileSize;

  // If limiting, skip to end minus maxBytes
  if (maxBytes > 0 && fileSize > maxBytes) {
    f.seek(fileSize - maxBytes);
    output.println(F("[... truncated ...]"));
  }

  // Stream in 64-byte chunks (stack-safe)
  char buf[64];
  size_t totalWritten = REDACTED
  while (f.available() && totalWritten < toRead) {
    size_t chunk = f.read((uint8_t*)buf, sizeof(buf) - 1);
    buf[chunk] = '\0';
    output.print(buf);
    totalWritten += REDACTED
  }
  f.close();
  return totalWritten;
}

void CrashHandler::clearLog() {
  LittleFS.remove(Paths::CRASH_LOG);
}

bool CrashHandler::hasCrashLog() {
  return LittleFS.exists(Paths::CRASH_LOG);
}