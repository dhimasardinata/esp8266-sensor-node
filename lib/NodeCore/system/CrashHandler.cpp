#include "system/CrashHandler.h"

#include <LittleFS.h>
#include <cstring>
#include <user_interface.h>  // For struct rst_info

#include "storage/Paths.h"
#include "generated/node_config.h"  // For FIRMWARE_VERSION

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

  PGM_P getResetReason_P(uint32_t reason) {
    switch (reason) {
      case REASON_WDT_RST:
        return PSTR("Hardware WDT");
      case REASON_EXCEPTION_RST:
        return PSTR("Fatal Exception");
      case REASON_SOFT_WDT_RST:
        return PSTR("Software WDT");
      case REASON_SOFT_RESTART:
        return PSTR("ESP.restart");
      default:
        return PSTR("Unknown");
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
  char reason[20];
  strncpy_P(reason, getResetReason_P(rst->reason), sizeof(reason) - 1);
  reason[sizeof(reason) - 1] = '\0';

  f.println(F("\n--- CRASH REPORT ---"));
  f.printf_P(PSTR("FW: %s (GH%d-N%d)\n"), FIRMWARE_VERSION, GH_ID, NODE_ID);
  f.printf_P(PSTR("Reason: %s (%d)\n"), reason, rst->reason);

  if (rst->reason == REASON_EXCEPTION_RST) {
    f.printf_P(PSTR("Ex:%d EPC1:0x%08x EPC2:0x%08x EPC3:0x%08x\n"),
               rst->exccause,
               rst->epc1,
               rst->epc2,
               rst->epc3);
    f.printf_P(PSTR("EXCV:0x%08x DEPC:0x%08x\n"), rst->excvaddr, rst->depc);
  }

  f.println(F("--------------------"));
  f.close();
}

// OOM Protection: Do NOT read the entire file if it's large.
String CrashHandler::getLog() {
  if (!LittleFS.exists(Paths::CRASH_LOG))
    return String(F("No logs."));
  File f = LittleFS.open(Paths::CRASH_LOG, "r");
  if (!f)
    return String(F("Error opening log."));

  // Safety: Only read the LAST 1024 bytes directly
  size_t size = f.size();
  const bool truncated = size > 1024;
  size_t toRead = truncated ? 1024 : size;
  if (truncated) {
    f.seek(size - toRead);
  }

  String s;
  const size_t prefixLen = sizeof("[...truncated...] ") - 1;
  if (!s.reserve(toRead + (truncated ? prefixLen : 0))) {
    f.close();
    return String(F("Error reading log."));
  }

  if (truncated) {
    s += F("[...truncated...] ");
  }

  char chunk[64];
  while (toRead > 0) {
    size_t chunkLen = (toRead < sizeof(chunk)) ? toRead : sizeof(chunk);
    size_t got = f.readBytes(chunk, chunkLen);
    if (got == 0) {
      break;
    }
    s.concat(chunk, got);
    toRead -= got;
  }
  f.close();

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

  // Stream complete crash reports only. If the newest report was interrupted
  // while being appended, drop that trailing partial block from terminal output.
  char buf[64];
  char line[96];
  char report[512];
  size_t totalWritten = REDACTED
  size_t lineLen = 0;
  size_t reportLen = 0;
  bool reportHasFooter = false;
  while (f.available() && totalWritten < toRead) {
    size_t chunk = f.read((uint8_t*)buf, sizeof(buf));
    if (chunk == 0) {
      break;
    }

    for (size_t i = 0; i < chunk && totalWritten < toRead; ++i, ++totalWritten) {
      const char ch = buf[i];

      if (reportLen + 1 < sizeof(report)) {
        report[reportLen++] = ch;
        report[reportLen] = '\0';
      }

      if (lineLen + 1 < sizeof(line)) {
        line[lineLen++] = ch;
        line[lineLen] = '\0';
      }

      if (ch != '\n') {
        continue;
      }

      size_t normalizedLen = lineLen;
      while (normalizedLen > 0 && (line[normalizedLen - 1] == '\n' || line[normalizedLen - 1] == '\r')) {
        --normalizedLen;
      }
      line[normalizedLen] = '\0';

      if (strcmp(line, "--------------------") == 0) {
        reportHasFooter = true;
      }

      if (reportHasFooter) {
        output.write(reinterpret_cast<const uint8_t*>(report), reportLen);
        reportLen = 0;
        report[0] = '\0';
        reportHasFooter = false;
      }

      lineLen = 0;
      line[0] = '\0';
    }
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
