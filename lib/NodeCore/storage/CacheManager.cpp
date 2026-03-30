#include "storage/CacheManager.h"

#include <system/ConfigManager.h>
#include <FS.h>
#include <LittleFS.h>

#include <cstring>
#include <memory>

#include "system/Logger.h"
#include "storage/Paths.h"
#include "support/Crc32.h"

#define CACHE_MAGIC 0xDEADBEEF

#ifndef CACHE_VERIFY_WRITE
#define CACHE_VERIFY_WRITE 0
#endif

struct CacheHeader {
  uint32_t magic;
  uint32_t head;
  uint32_t tail;
  uint32_t size;
  uint16_t version;
  uint32_t crc;
};

static CacheHeader cacheHeader;
const uint32_t CACHE_DATA_START = sizeof(CacheHeader);

// =========================================================================
// == FORWARD DECLARATIONS & STATIC HELPERS (MUST BE TOP)
// =========================================================================

enum class ScanResult { FOUND, NEED_MORE, EMPTY };
static constexpr uint32_t SYNC_SCAN_BUDGET_BYTES = 1024;
static constexpr uint32_t TRIM_BUDGET_BYTES = 2048;
static constexpr uint16_t CACHE_FLUSH_MUTATION_THRESHOLD = 32;
static constexpr unsigned long CACHE_FLUSH_MAX_DELAY_MS = 120000UL;
static ScanResult performSyncScan(File& cacheFile, uint32_t budgetBytes);
static void advanceTailPointer(uint32_t total_record_size);
static void updateHeadPointer(uint32_t total_len);
static bool trimCacheForWrite(File& cacheFile, uint32_t total_len_on_disk);

// CRC32 implementation is shared via Crc32.{h,cpp}.

// Constant for record validation tolerance
static constexpr uint16_t RECORD_LEN_TOLERANCE = 100;
static constexpr uint16_t RECORD_MAGIC = 0xA55A;  // Active Error Correction Sync Marker

// FORMAL VERIFICATION: Compile-Time Invariants
static_assert(MAX_PAYLOAD_SIZE + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(RECORD_MAGIC) < MAX_CACHE_DATA_SIZE,
              "FATAL: Record size exceeds Cache Size. Pointer arithmetic will fail.");
static_assert(MAX_CACHE_DATA_SIZE < 0xFFFFFFFF, "FATAL: Cache size must fit in uint32_t.");

static uint32_t calculate_header_crc(const CacheHeader& header) {
  return Crc32::compute((const uint8_t*)&header, offsetof(CacheHeader, crc));
}

static size_t readWithWrap(File& file, uint32_t pos, uint8_t* buf, size_t len) {
  if (pos < CACHE_DATA_START)
    return 0;

  pos = CACHE_DATA_START + (pos - CACHE_DATA_START) % MAX_CACHE_DATA_SIZE;
  file.seek(pos);

  uint32_t space_before_wrap = (CACHE_DATA_START + MAX_CACHE_DATA_SIZE) - pos;
  if (len <= space_before_wrap) {
    return file.read(buf, len);
  } else {
    size_t bytes_read = file.read(buf, space_before_wrap);
    file.seek(CACHE_DATA_START);
    // Only continue reading if we got what we requested
    if (bytes_read == space_before_wrap) {
      bytes_read += file.read(buf + space_before_wrap, len - space_before_wrap);
    }
    return bytes_read;
  }
}

static size_t writeWithWrap(File& file, uint32_t pos, const uint8_t* buf, size_t len) {
  if (pos < CACHE_DATA_START)
    return 0;

  pos = CACHE_DATA_START + (pos - CACHE_DATA_START) % MAX_CACHE_DATA_SIZE;
  file.seek(pos);

  uint32_t space_before_wrap = (MAX_CACHE_DATA_SIZE + CACHE_DATA_START) - pos;
  if (len <= space_before_wrap) {
    return file.write(buf, len);
  } else {
    size_t first_written = file.write(buf, space_before_wrap);
    if (first_written != space_before_wrap) {
      return first_written;
    }
    file.seek(CACHE_DATA_START);
    size_t second_written = file.write(buf + space_before_wrap, len - space_before_wrap);
    return first_written + second_written;
  }
}

#if CACHE_VERIFY_WRITE
static bool verifyWithWrap(File& file, uint32_t pos, const uint8_t* expectedBuf, size_t len) {
  if (pos < CACHE_DATA_START)
    return false;

  // Align position to wrap boundary immediately
  uint32_t logical_pos = CACHE_DATA_START + (pos - CACHE_DATA_START) % MAX_CACHE_DATA_SIZE;

  uint8_t verifyBuf[64];
  size_t remaining = len;
  size_t offset = 0;

  file.seek(logical_pos);

  while (remaining > 0) {
    size_t chunk = (remaining > sizeof(verifyBuf)) ? sizeof(verifyBuf) : remaining;

    // Use readWithWrap logic to fetch data back from disk
    // pos + offset ensures we trace the logical stream correctly through the wrap
    if (readWithWrap(file, pos + offset, verifyBuf, chunk) != chunk) {
      return false;  // Read error
    }

    if (memcmp(expectedBuf + offset, verifyBuf, chunk) != 0) {
      return false;  // Data Mismatch!
    }

    remaining -= chunk;
    offset += chunk;
  }
  return true;
}
#endif  // CACHE_VERIFY_WRITE

static bool readCacheHeader(File& file) {
  if (!file)
    return false;
  file.seek(0);
  return (file.read((uint8_t*)&cacheHeader, sizeof(CacheHeader)) == sizeof(CacheHeader));
}

static bool writeCacheHeader(File& file) {
  if (!file)
    return false;
  cacheHeader.crc = calculate_header_crc(cacheHeader);
  file.seek(0);
  return (file.write((uint8_t*)&cacheHeader, sizeof(CacheHeader)) == sizeof(CacheHeader));
}

// --- Additional helpers for complexity reduction ---

static bool writeRecordData(
    File& file, uint32_t start_pos, const char* data, uint16_t record_len, uint32_t payload_crc) {
  uint32_t cursor = start_pos;

  // 1. Magic (Sync Marker)
  uint16_t magic = RECORD_MAGIC;
  if (writeWithWrap(file, cursor, (const uint8_t*)&magic, sizeof(magic)) != sizeof(magic))
    return false;
  cursor += sizeof(magic);

  // 2. Length
  if (writeWithWrap(file, cursor, (const uint8_t*)&record_len, sizeof(record_len)) != sizeof(record_len))
    return false;
  cursor += sizeof(record_len);

  // 3. Data
  if (writeWithWrap(file, cursor, (const uint8_t*)data, record_len) != record_len)
    return false;
  cursor += record_len;

  // 4. CRC
  if (writeWithWrap(file, cursor, (const uint8_t*)&payload_crc, sizeof(payload_crc)) != sizeof(payload_crc))
    return false;

  return true;
}

#if CACHE_VERIFY_WRITE
static bool verifyRecordData(
    File& file, uint32_t start_pos, const char* data, uint16_t record_len, uint32_t payload_crc) {
  if (!verifyWithWrap(file, start_pos, (const uint8_t*)&RECORD_MAGIC, sizeof(RECORD_MAGIC)))
    return false;

  uint32_t len_offset = start_pos + sizeof(RECORD_MAGIC);
  if (!verifyWithWrap(file, len_offset, (const uint8_t*)&record_len, sizeof(record_len)))
    return false;

  uint32_t data_offset = len_offset + sizeof(record_len);
  if (!verifyWithWrap(file, data_offset, (const uint8_t*)data, record_len))
    return false;

  uint32_t crc_offset = data_offset + record_len;
  if (!verifyWithWrap(file, crc_offset, (const uint8_t*)&payload_crc, sizeof(payload_crc)))
    return false;

  return true;
}
#endif  // CACHE_VERIFY_WRITE

static void updateHeadPointer(uint32_t total_len) {
  uint32_t final_pos = cacheHeader.head + total_len;
  if (final_pos >= (CACHE_DATA_START + MAX_CACHE_DATA_SIZE)) {
    cacheHeader.head = CACHE_DATA_START + (final_pos - (CACHE_DATA_START + MAX_CACHE_DATA_SIZE));
  } else {
    cacheHeader.head = final_pos;
  }
  cacheHeader.size += total_len;

  // RUNTIME INVARIANT CHECK (Formal Safety)
  if (cacheHeader.head < CACHE_DATA_START || cacheHeader.head >= CACHE_DATA_START + MAX_CACHE_DATA_SIZE) {
    LOG_ERROR("CACHE", F("CRITICAL: Head out of bounds (0x%08X). Resetting."), cacheHeader.head);
    cacheHeader.head = CACHE_DATA_START;
    cacheHeader.tail = CACHE_DATA_START;
    cacheHeader.size = 0;
  }
}

static void advanceTailPointer(uint32_t total_record_size) {
  uint32_t new_tail = cacheHeader.tail + total_record_size;
  if (new_tail >= MAX_CACHE_DATA_SIZE + CACHE_DATA_START) {
    cacheHeader.tail = CACHE_DATA_START + (new_tail - (MAX_CACHE_DATA_SIZE + CACHE_DATA_START));
  } else {
    cacheHeader.tail = new_tail;
  }

  // RUNTIME INVARIANT CHECK (Formal Safety)
  if (cacheHeader.tail < CACHE_DATA_START || cacheHeader.tail >= CACHE_DATA_START + MAX_CACHE_DATA_SIZE) {
    LOG_ERROR("CACHE", F("CRITICAL INVARIANT VIOLATION: Tail out of bounds (0x%08X). Resetting."), cacheHeader.tail);
    // We cannot call resetImpl() here because it's static and needs instance context?
    // Actually resetImpl accesses static cacheHeader but needs m_file object.
    // Since this is critical failure, we force size=0 (Empty) to prevent OOB access.
    cacheHeader.head = CACHE_DATA_START;
    cacheHeader.tail = CACHE_DATA_START;
    cacheHeader.size = 0;
    return;
  }

  if (cacheHeader.size < total_record_size) {
    cacheHeader.size = 0;
  } else {
    cacheHeader.size -= total_record_size;
  }

  if (cacheHeader.size == 0) {
    cacheHeader.head = CACHE_DATA_START;
    cacheHeader.tail = CACHE_DATA_START;
  }
}

// Returns true if Magic Byte found (Tail aligned). False if cache empty/read-error.
// Returns true if Magic Byte found. False if cache empty/read-error.
static ScanResult performSyncScan(File& cacheFile, uint32_t budgetBytes) {
  LOG_WARN("CACHE", F("Sync: Scanning for next record (Buffered)..."));

  // Use a small stack buffer for scanning to drastically reduce FS call overhead
  constexpr size_t SCAN_BUF_SIZE = 64;
  uint8_t buf[SCAN_BUF_SIZE];
  uint32_t bytesScanned = 0;
  uint32_t wdtBytesSinceYield = 0;
  const bool unlimited = (budgetBytes == 0);

  // We scan until we find MAGIC or run out of data
  // record MAGIC is 2 bytes.

  while (cacheHeader.size > sizeof(RECORD_MAGIC)) {
    // Determine how much to read (up to local buffer size)
    // We must read at least sizeof(magic) to check.
    size_t chunk = std::min((size_t)cacheHeader.size, SCAN_BUF_SIZE);
    if (!unlimited) {
      if (bytesScanned >= budgetBytes)
        return ScanResult::NEED_MORE;
      uint32_t remaining = budgetBytes - bytesScanned;
      if (remaining < 2)
        return ScanResult::NEED_MORE;
      size_t maxChunk = static_cast<size_t>(remaining + 1);
      if (chunk > maxChunk)
        chunk = maxChunk;
    }

    // Helper: Peek/Read without advancing tail permanently yet?
    // Reading advances the file pointer, but our "logical" tail is manual.
    // readWithWrap reads from 'logical tail'.

    // Logic: Read chunk from current tail.
    size_t actual = readWithWrap(cacheFile, cacheHeader.tail, buf, chunk);
    if (actual < sizeof(RECORD_MAGIC))
      return ScanResult::EMPTY;  // Should not happen given buffer check

    // Scan buffer
    // We can check up to actual - 1 (since magic is 2 bytes)
    constexpr uint8_t kMagicLo = static_cast<uint8_t>(RECORD_MAGIC & 0xFF);
    constexpr uint8_t kMagicHi = static_cast<uint8_t>((RECORD_MAGIC >> 8) & 0xFF);
    const uint8_t* cur = buf;
    const uint8_t* end = buf + (actual - 1);
    while (cur < end) {
      const uint8_t* hit = static_cast<const uint8_t*>(memchr(cur, kMagicLo, end - cur));
      if (!hit) {
        break;
      }
      if (hit[1] == kMagicHi) {
        size_t i = static_cast<size_t>(hit - buf);
        // FOUND!
        // Advance tail by 'i' bytes to align exactly on Magic
        advanceTailPointer(i);
        LOG_INFO("CACHE", F("Sync: Found Magic at offset +%u"), i);
        return ScanResult::FOUND;
      }
      cur = hit + 1;
    }

    // Not found in this chunk.
    // We must advance. But be careful about straddling.
    // IF magic was [byte 63] [byte 64] (assuming buf 64),
    // we checked up to index 62 (buf[62], buf[63]).
    // We did NOT check (buf[63], next_read[0]).
    // So we can safely advance by (actual - 1) bytes.
    // Leaving the last byte to be the first byte of next read.

    size_t step = actual - 1;
    advanceTailPointer(step);
    bytesScanned += step;
    if (!unlimited && bytesScanned >= budgetBytes)
      return ScanResult::NEED_MORE;

    wdtBytesSinceYield += static_cast<uint32_t>(step);
    if (wdtBytesSinceYield > 2048) {
      wdtBytesSinceYield = 0;
      yield();
    }
  }

  // If we are here, we exhausted the cache scanning for magic.
  if (cacheHeader.size <= sizeof(RECORD_MAGIC)) {
    LOG_WARN("CACHE", F("Sync: Failed. Cache exhausted."));
    advanceTailPointer(cacheHeader.size);  // Clear all
  }
  return ScanResult::EMPTY;
}

// =========================================================================
// == CLASS IMPLEMENTATION
// =========================================================================

CacheManager::CacheManager() {}

void CacheManager::initImpl() {
  if (m_file)
    return;  // Already open

  if (!LittleFS.exists(Paths::CACHE_FILE)) {
    m_file = LittleFS.open(Paths::CACHE_FILE, "w+");
    if (!m_file) {
      LOG_ERROR("CACHE", F("Failed to create cache file!"));
      return;
    }

    cacheHeader.magic = CACHE_MAGIC;
    cacheHeader.version = 4;  // Version 4: Added Magic Byte
    cacheHeader.head = CACHE_DATA_START;
    cacheHeader.tail = CACHE_DATA_START;
    cacheHeader.size = 0;
    if (!writeCacheHeader(m_file)) {
      return;
    }
  } else {
    m_file = LittleFS.open(Paths::CACHE_FILE, "r+");
    if (!m_file) {
      LOG_ERROR("CACHE", F("Failed to open existing cache!"));
      return;
    }

    if (!readCacheHeader(m_file) || cacheHeader.magic != CACHE_MAGIC || cacheHeader.version < 4 ||
        calculate_header_crc(cacheHeader) != cacheHeader.crc) {
      LOG_ERROR("CACHE", F("Cache header invalid. Resetting."));
      m_file.close();
      resetImpl();
      return;
    }
  }
  m_dirty = false;
  m_pendingMutations = 0;
  m_lastFlushMs = millis();
  LOG_INFO("CACHE", F("Init OK. Size: %u bytes"), cacheHeader.size);
}

void CacheManager::resetImpl() {
  LOG_WARN("CACHE", F("Resetting cache file..."));
  if (m_file)
    m_file.close();
  LittleFS.remove(Paths::CACHE_FILE);
  initImpl();
}

void CacheManager::flush() {
  if (!m_dirty || !m_file)
    return;

  if (writeCacheHeader(m_file)) {
    m_file.flush();
    m_dirty = false;
    m_pendingMutations = 0;
    m_lastFlushMs = millis();
    // LOG_DEBUG("CACHE", F("Header flushed."));
  }
}

void CacheManager::markDirty() {
  m_dirty = true;
  if (m_pendingMutations < 0xFFFFu) {
    m_pendingMutations++;
  }
  const unsigned long now = millis();
  const bool timedOut = (m_lastFlushMs == 0 || now < m_lastFlushMs || (now - m_lastFlushMs) >= CACHE_FLUSH_MAX_DELAY_MS);
  if (m_pendingMutations >= CACHE_FLUSH_MUTATION_THRESHOLD || timedOut) {
    flush();
  }
}

// =============================================================================
// write() Helper Functions (Reduce Complexity)
// =============================================================================

// Pass file handle to prevent race conditions during file operations.
// Returns true if cache was trimmed successfully.
static bool trimCacheForWrite(File& cacheFile, uint32_t total_len_on_disk) {
  if (cacheHeader.size + total_len_on_disk <= REDACTED
    return true;

  uint32_t bytesNeeded = (cacheHeader.size + total_len_on_disk) - MAX_CACHE_DATA_SIZE;
  uint32_t bytesTrimmed = 0;
  // Keep popping records until we have space or cache is empty
  while (bytesNeeded > 0 && cacheHeader.size > 0) {
    ESP.wdtFeed();  // Fix 6: Prevent WDT during heavy trimming loops
    // 1. Read Magic
    uint16_t magic;
    if (readWithWrap(cacheFile, cacheHeader.tail, (uint8_t*)&magic, sizeof(magic)) != sizeof(magic)) {
      // Physical Read Error. Do NOT abort.
      // Skip 1 byte to try and find readable area.
      LOG_WARN("CACHE", F("Trim: Read Error. Skipping 1 byte."));
      advanceTailPointer(1);
      bytesNeeded = (bytesNeeded > 1) ? (bytesNeeded - 1) : 0;
      bytesTrimmed += 1;
      if (bytesTrimmed >= TRIM_BUDGET_BYTES) {
        LOG_WARN("CACHE", F("Trim budget hit; deferring remaining trim."));
        return false;
      }
      continue;
    }

    // ACTIVE ERROR CORRECTION: Sync Loss Detection
    if (magic != RECORD_MAGIC) {
      LOG_WARN("CACHE", F("Trim: Sync Loss (0x%04X). Resyncing..."), magic);
      ScanResult scan = performSyncScan(cacheFile, SYNC_SCAN_BUDGET_BYTES);
      if (scan == ScanResult::FOUND) {
        // Found valid record!
        // CRITICAL FIX: Do NOT pop it immediately.
        // Continue loop to check if we actually NEED space.
        // The scan effectively removed the garbage bytes.
        bytesNeeded = (cacheHeader.size + total_len_on_disk > MAX_CACHE_DATA_SIZE)
                          ? (cacheHeader.size + total_len_on_disk - MAX_CACHE_DATA_SIZE)
                          : 0;
        continue;
      } else if (scan == ScanResult::NEED_MORE) {
        LOG_WARN("CACHE", F("Trim budget hit during resync; deferring."));
        return false;
      } else {
        // Scan failed or cache empty. Loop terminates.
        continue;
      }
    }

    // 2. Read Length (Offset by Magic)
    uint16_t record_len;
    if (readWithWrap(cacheFile, cacheHeader.tail + sizeof(RECORD_MAGIC), (uint8_t*)&record_len, sizeof(record_len)) !=
        sizeof(record_len)) {
      return false;
    }

    // Sanity check length
    if (record_len == 0 || record_len > MAX_PAYLOAD_SIZE) {
      // Length corrupt? Treat as Sync Loss and scan next byte
      LOG_WARN("CACHE", F("Corrupt Len %u. Resyncing..."), record_len);
      advanceTailPointer(1);
      bytesNeeded = (bytesNeeded > 1) ? (bytesNeeded - 1) : 0;
      bytesTrimmed += 1;
      if (bytesTrimmed >= TRIM_BUDGET_BYTES) {
        LOG_WARN("CACHE", F("Trim budget hit; deferring remaining trim."));
        return false;
      }
      continue;
    }

    uint32_t total_record_size = REDACTED
    advanceTailPointer(total_record_size);
    bytesTrimmed += total_record_size;
    if (bytesNeeded > total_record_size) {
      bytesNeeded -= total_record_size;
    } else {
      bytesNeeded = 0;
    }
    if (bytesTrimmed >= TRIM_BUDGET_BYTES && bytesNeeded > 0) {
      LOG_WARN("CACHE", F("Trim budget hit; deferring remaining trim."));
      return false;
    }

    // Lazy update - don't write header here (optimized)
  }
  return true;
}
// Returns true if write succeeded with verification
static bool tryWriteWithRetry(File& cacheFile, const char* data, uint16_t record_len, uint32_t payload_crc) {
  constexpr int MAX_RETRIES = 3;

  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    ESP.wdtFeed();

    // 1. Write Data
    if (!writeRecordData(cacheFile, cacheHeader.head, data, record_len, payload_crc)) {
      continue;  // Write failed (e.g. FS full or hardware error), retry
    }

    // 2. Validate (Read-Back) - MAXIMUM SAFETY
#if CACHE_VERIFY_WRITE
    cacheFile.flush();  // Ensure physically on media before reading back
    if (verifyRecordData(cacheFile, cacheHeader.head, data, record_len, payload_crc)) {
      return true;  // Success!
    }

    LOG_WARN("CACHE", F("Write verify failed (Attempt %d/%d). Retrying..."), attempt, MAX_RETRIES);
    delay(20);
#else
    return true;
#endif
  }

  LOG_ERROR("CACHE", F("Write Failed. Flash sector likely dead."));
  return false;
}

// =============================================================================
// Main Write Operation
// =============================================================================

bool CacheManager::writeImpl(const char* data, uint16_t len) {
  if (len == 0)
    return true;
  if (!m_file)
    initImpl();
  if (!m_file)
    return false;

  uint16_t record_len = len;
  uint32_t payload_crc = Crc32::compute((const uint8_t*)data, record_len);
  uint32_t total_len_on_disk = REDACTED

  // OPTIMIZATION: Removed hasFilesystemSpace check (O(N) overhead)

  if (!trimCacheForWrite(m_file, total_len_on_disk)) {
    LOG_ERROR("CACHE", F("Failed to trim cache (Full/Corrupt). Write aborted."));
    // NO RESET. Preserve existing data.
    // If the cache is physically broken or full of untouchable garbage, we just stop writing new data.
    // Old data remains readable (hopefully) via pop_one.
    return false;
  }

  if (cacheHeader.size + total_len_on_disk > MAX_CACHE_DATA_SIZE) {
    LOG_ERROR("CACHE", F("Record is too large to fit in cache."));
    return false;
  }

  // Write with retry
  if (!tryWriteWithRetry(m_file, data, record_len, payload_crc)) {
    return false;
  }

  updateHeadPointer(total_len_on_disk);

  // POWER SAFETY: Write header but DELAY flushing to reduce wear
  // We only set the dirty flag. The background timer (30min) or shutdown will flush.
  // if (writeCacheHeader(m_file)) {
  // m_file.flush();
  // }
  markDirty();
  return true;
}

CacheReadError CacheManager::read_oneImpl(char* out_buffer, size_t buffer_size, size_t& out_len) {
  out_len = 0;
  if (cacheHeader.size == 0) {
    return CacheReadError::CACHE_EMPTY;
  }

  if (!m_file)
    initImpl();
  if (!m_file)
    return CacheReadError::FILE_READ_ERROR;

  // 1. Verify Magic
  uint16_t magic;
  if (readWithWrap(m_file, cacheHeader.tail, (uint8_t*)&magic, sizeof(magic)) != sizeof(magic)) {
    return CacheReadError::FILE_READ_ERROR;
  }

  if (magic != RECORD_MAGIC) {
    // --- DEEP RECOVERY HEURISTIC (User Request: "Chance to be corrected") ---
    // Maybe only the Magic Byte is corrupt (Bit Rot), but data is fine?
    // Let's blindly try to verify the rest of the record using CRC.

    bool salvaged = false;
    uint16_t presumed_len;
    // Peek at length (offset 2 bytes)
    if (readWithWrap(m_file, cacheHeader.tail + sizeof(RECORD_MAGIC), (uint8_t*)&presumed_len, sizeof(presumed_len)) ==
        sizeof(presumed_len)) {
      if (presumed_len > 0 && presumed_len <= MAX_PAYLOAD_SIZE) {
        // Length looks sanity. Let's try to verify CRC.
        // We need to read presumed data + CRC
        uint32_t check_offset = cacheHeader.tail + sizeof(RECORD_MAGIC) + sizeof(presumed_len);
        uint32_t stored_crc_offset = check_offset + presumed_len;
        uint32_t stored_crc;

        // Read stored CRC
        if (readWithWrap(m_file, stored_crc_offset, (uint8_t*)&stored_crc, sizeof(stored_crc)) == sizeof(stored_crc)) {
          // We can't really "verify" CRC without reading data into buffer.
          // But we are in read_oneImpl, so we HAVE the buffer!
          if (presumed_len <= buffer_size) {
            readWithWrap(m_file, check_offset, (uint8_t*)out_buffer, presumed_len);
            uint32_t calc_crc = Crc32::compute((const uint8_t*)out_buffer, presumed_len);
            if (calc_crc == stored_crc) {
              LOG_WARN("CACHE", F("Deep Recovery: Magic corrupt (0x%04X) but CRC OK! Salvaging."), magic);
              out_len = presumed_len;
              salvaged = true;
            }
          }
        }
      }
    }

    if (!salvaged) {
      // Corruption (True Garbage).
      // We must SKIP this specific bad area to find the next valid record.
      LOG_WARN("CACHE", F("Read: Sync Loss & Recovery Failed. Resyncing..."));

      ScanResult scan = performSyncScan(m_file, SYNC_SCAN_BUDGET_BYTES);
      if (scan == ScanResult::FOUND)
        return CacheReadError::CORRUPT_DATA;
      if (scan == ScanResult::NEED_MORE)
        return CacheReadError::SCANNING;
      return CacheReadError::CACHE_EMPTY;
    } else {
      // SALVAGED! proceed to return success.
      return CacheReadError::NONE;
    }
  }

  uint16_t record_len;
  if (readWithWrap(m_file, cacheHeader.tail + sizeof(RECORD_MAGIC), (uint8_t*)&record_len, sizeof(record_len)) !=
      sizeof(record_len)) {
    LOG_ERROR("CACHE", F("storage/CacheManager::read_one: Failed to read record length."));
    return CacheReadError::FILE_READ_ERROR;
  }

  if (record_len == 0 || record_len > MAX_PAYLOAD_SIZE) {
    LOG_ERROR("CACHE",
              F("storage/CacheManager::read_one: Invalid record length %u (Max: %u). Discarding corrupted record."),
              record_len,
              MAX_PAYLOAD_SIZE);
    (void)pop_one();
    return CacheReadError::CORRUPT_DATA;
  }

  if (record_len > buffer_size) {
    LOG_ERROR(
        "CACHE", F("storage/CacheManager::read_one: Provided buffer is too small. Needed %u, have %zu"), record_len, buffer_size);
    return CacheReadError::OUT_OF_MEMORY;
  }

  uint32_t payload_offset = cacheHeader.tail + sizeof(RECORD_MAGIC) + sizeof(record_len);
  size_t bytes_read = readWithWrap(m_file, payload_offset, (uint8_t*)out_buffer, record_len);

  if (bytes_read != record_len) {
    LOG_ERROR(
        "CACHE", F("storage/CacheManager::read_one: Mismatch in bytes read. Expected %u, got %zu"), record_len, bytes_read);
    return CacheReadError::FILE_READ_ERROR;
  }

  uint32_t stored_crc;
  uint32_t crc_offset = payload_offset + record_len;
  if (readWithWrap(m_file, crc_offset, (uint8_t*)&stored_crc, sizeof(stored_crc)) != sizeof(stored_crc)) {
    LOG_ERROR("CACHE", F("storage/CacheManager::read_one: Failed to read stored CRC."));
    return CacheReadError::FILE_READ_ERROR;
  }

  uint32_t calculated_crc = Crc32::compute((const uint8_t*)out_buffer, record_len);
  if (calculated_crc != stored_crc) {
    LOG_ERROR(
        "CACHE", F("CRC mismatch! Data corrupted. Stored: 0x%08X, Calc: 0x%08X. Discarding."), stored_crc, calculated_crc);
    (void)pop_oneImpl();
    return CacheReadError::CORRUPT_DATA;
  }

  out_len = record_len;
  return CacheReadError::NONE;
}

bool CacheManager::pop_oneImpl() {
  if (cacheHeader.size == 0)
    return true;

  if (!m_file)
    initImpl();
  if (!m_file)
    return false;

  // 1. Check Magic (Sync Logic)
  uint16_t magic;
  if (readWithWrap(m_file, cacheHeader.tail, (uint8_t*)&magic, sizeof(magic)) != sizeof(magic)) {
    // Physical read error? Skip 1 byte and try again next time.
    // Treat as "Popped/Skipped" to avoid infinite loops if the caller keeps calling pop.
    LOG_ERROR("CACHE", F("Pop: Physical Read Error. Skipping 1 byte."));
    advanceTailPointer(1);
    markDirty();
    return true;
  }

  if (magic != RECORD_MAGIC) {
    LOG_WARN("CACHE", F("Pop: Sync Loss. Resyncing..."));
    ScanResult scan = performSyncScan(m_file, SYNC_SCAN_BUDGET_BYTES);
    if (scan == ScanResult::FOUND) {
      // We found valid record. Proceed to pop it (Standard behavior)
      // Fall through to read-len and advance.
    } else {
      return true;  // Cache empty or scan pending; try again later.
    }
  }

  uint16_t record_len;
  if (readWithWrap(m_file, cacheHeader.tail + sizeof(RECORD_MAGIC), (uint8_t*)&record_len, sizeof(record_len)) !=
      sizeof(record_len)) {
    LOG_ERROR("CACHE", F("Pop: Len Read Fail. Skipping 1 byte."));
    advanceTailPointer(1);
    markDirty();
    return true;
  }

  if (record_len > MAX_PAYLOAD_SIZE + RECORD_LEN_TOLERANCE) {
    // If Length is bad, we treat it as Sync failure and advance 1 byte (scan)
    // But since we are "Popping", effectively we just advance 1 byte and return true (item removed/skipped).
    // Wait, if we advance 1 byte, we might find a valid record next call.
    LOG_WARN("CACHE", F("Pop: Bad Len %u. Skipping byte."), record_len);
    advanceTailPointer(1);
    markDirty();
    return true;
  }

  uint32_t total_record_size = REDACTED
  advanceTailPointer(total_record_size);

  // POWER SAFETY: Write header but DELAY flushing.
  // if (writeCacheHeader(m_file)) {
  // m_file.flush();
  // }
  markDirty();
  return true;
}

void CacheManager::get_statusImpl(uint32_t& size_bytes, uint32_t& head, uint32_t& tail) {
  size_bytes = cacheHeader.size;
  head = cacheHeader.head;
  tail = cacheHeader.tail;
}

uint32_t CacheManager::get_sizeImpl() {
  return cacheHeader.size;
}
