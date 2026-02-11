#include "CacheManager.h"

#include <ConfigManager.h>
#include <FS.h>
#include <LittleFS.h>

#include <cstring>
#include <memory>

#include "Logger.h"
#include "Paths.h"

#define CACHE_MAGIC 0xDEADBEEF

#ifndef CACHE_VERIFY_WRITE
#define CACHE_VERIFY_WRITE 0
#endif

#ifndef CACHE_CRC_TABLE_IN_RAM
#define CACHE_CRC_TABLE_IN_RAM 0
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
static ScanResult performSyncScan(File& cacheFile, uint32_t budgetBytes);
static void advanceTailPointer(uint32_t total_record_size);
static void updateHeadPointer(uint32_t total_len);
static bool trimCacheForWrite(File& cacheFile, uint32_t total_len_on_disk);

// CRC table stored in PROGMEM to conserve RAM.
static const uint32_t crc_table[256] PROGMEM = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3, 0x0EDB8832,
    0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856, 0x646BA8C0, 0xFD62F97A,
    0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C, 0xDBBBBBD6, 0xACBCCB40, 0x32D86CE3,
    0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB,
    0xB6662D3D, 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01, 0x6B6B51F4,
    0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65, 0x4DB26158, 0x3AB551CE, 0xA3BC0074,
    0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CD9, 0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525,
    0x206F85B3, 0xB966D409, 0xCE61E49F, 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615,
    0x73DC1683, 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7, 0xFED41B76,
    0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B, 0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6,
    0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7,
    0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7,
    0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45, 0xA00AE278,
    0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9, 0xBDBDF21C, 0xCABAC28A, 0x53B39330,
    0x24B4A3A6, 0xBAD03605, 0xCDD706B3, 0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};

// Constant for record validation tolerance
static constexpr uint16_t RECORD_LEN_TOLERANCE = 100;
static constexpr uint16_t RECORD_MAGIC = 0xA55A;  // Active Error Correction Sync Marker

// FORMAL VERIFICATION: Compile-Time Invariants
static_assert(MAX_PAYLOAD_SIZE + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(RECORD_MAGIC) < MAX_CACHE_DATA_SIZE,
              "FATAL: Record size exceeds Cache Size. Pointer arithmetic will fail.");
static_assert(MAX_CACHE_DATA_SIZE < 0xFFFFFFFF, "FATAL: Cache size must fit in uint32_t.");

static uint32_t calculate_crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFL;
#if CACHE_CRC_TABLE_IN_RAM
  static uint32_t crc_table_ram[256];
  static bool crc_table_ready = false;
  if (!crc_table_ready) {
    for (size_t i = 0; i < 256; ++i) {
      crc_table_ram[i] = pgm_read_dword(&crc_table[i]);
    }
    crc_table_ready = true;
  }
#endif
  for (size_t i = 0; i < length; i++) {
#if CACHE_CRC_TABLE_IN_RAM
    crc = crc_table_ram[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
#else
    crc = pgm_read_dword(&crc_table[(crc ^ data[i]) & 0xFF]) ^ (crc >> 8);
#endif
  }
  return crc ^ 0xFFFFFFFFL;
}

static uint32_t calculate_header_crc(const CacheHeader& header) {
  return calculate_crc32((const uint8_t*)&header, offsetof(CacheHeader, crc));
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
    size_t bytes_written = file.write(buf, space_before_wrap);
    file.seek(CACHE_DATA_START);
    bytes_written += file.write(buf + space_before_wrap, len - space_before_wrap);
    return bytes_written;
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
    LOG_ERROR("CACHE", "CRITICAL: Head out of bounds (0x%08X). Resetting.", cacheHeader.head);
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
    LOG_ERROR("CACHE", "CRITICAL INVARIANT VIOLATION: Tail out of bounds (0x%08X). Resetting.", cacheHeader.tail);
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
  LOG_WARN("CACHE", "Sync: Scanning for next record (Buffered)...");

  // Use a small stack buffer for scanning to drastically reduce FS call overhead
  constexpr size_t SCAN_BUF_SIZE = 64;
  uint8_t buf[SCAN_BUF_SIZE];
  uint32_t bytesScanned = 0;
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

    // WDT Feed
    if (bytesScanned > 2048) {
      bytesScanned = 0;
      yield();
    }
  }

  // If we are here, we exhausted the cache scanning for magic.
  if (cacheHeader.size <= sizeof(RECORD_MAGIC)) {
    LOG_WARN("CACHE", "Sync: Failed. Cache exhausted.");
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
      LOG_ERROR("CACHE", "Failed to create cache file!");
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
      LOG_ERROR("CACHE", "Failed to open existing cache!");
      return;
    }

    if (!readCacheHeader(m_file) || cacheHeader.magic != CACHE_MAGIC || cacheHeader.version < 4 ||
        calculate_header_crc(cacheHeader) != cacheHeader.crc) {
      LOG_ERROR("CACHE", "Cache header invalid. Resetting.");
      m_file.close();
      resetImpl();
      return;
    }
  }
  LOG_INFO("CACHE", "Init OK. Size: %u bytes", cacheHeader.size);
}

void CacheManager::resetImpl() {
  LOG_WARN("CACHE", "Resetting cache file...");
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
    // LOG_DEBUG("CACHE", F("Header flushed."));
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
      LOG_WARN("CACHE", "Trim: Read Error. Skipping 1 byte.");
      advanceTailPointer(1);
      bytesNeeded = (bytesNeeded > 1) ? (bytesNeeded - 1) : 0;
      bytesTrimmed += 1;
      if (bytesTrimmed >= TRIM_BUDGET_BYTES) {
        LOG_WARN("CACHE", "Trim budget hit; deferring remaining trim.");
        return false;
      }
      continue;
    }

    // ACTIVE ERROR CORRECTION: Sync Loss Detection
    if (magic != RECORD_MAGIC) {
      LOG_WARN("CACHE", "Trim: Sync Loss (0x%04X). Resyncing...", magic);
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
        LOG_WARN("CACHE", "Trim budget hit during resync; deferring.");
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
      LOG_WARN("CACHE", "Corrupt Len %u. Resyncing...", record_len);
      advanceTailPointer(1);
      bytesNeeded = (bytesNeeded > 1) ? (bytesNeeded - 1) : 0;
      bytesTrimmed += 1;
      if (bytesTrimmed >= TRIM_BUDGET_BYTES) {
        LOG_WARN("CACHE", "Trim budget hit; deferring remaining trim.");
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
      LOG_WARN("CACHE", "Trim budget hit; deferring remaining trim.");
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

  LOG_ERROR("CACHE", "Write Failed. Flash sector likely dead.");
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
  uint32_t payload_crc = calculate_crc32((const uint8_t*)data, record_len);
  uint32_t total_len_on_disk = REDACTED

  // OPTIMIZATION: Removed hasFilesystemSpace check (O(N) overhead)

  if (!trimCacheForWrite(m_file, total_len_on_disk)) {
    LOG_ERROR("CACHE", "Failed to trim cache (Full/Corrupt). Write aborted.");
    // NO RESET. Preserve existing data.
    // If the cache is physically broken or full of untouchable garbage, we just stop writing new data.
    // Old data remains readable (hopefully) via pop_one.
    return false;
  }

  if (cacheHeader.size + total_len_on_disk > MAX_CACHE_DATA_SIZE) {
    LOG_ERROR("CACHE", "Record is too large to fit in cache.");
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
  m_dirty = true;
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
            uint32_t calc_crc = calculate_crc32((const uint8_t*)out_buffer, presumed_len);
            if (calc_crc == stored_crc) {
              LOG_WARN("CACHE", "Deep Recovery: Magic corrupt (0x%04X) but CRC OK! Salvaging.", magic);
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
      LOG_WARN("CACHE", "Read: Sync Loss & Recovery Failed. Resyncing...");

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
    LOG_ERROR("CACHE", "CacheManager::read_one: Failed to read record length.");
    return CacheReadError::FILE_READ_ERROR;
  }

  if (record_len == 0 || record_len > MAX_PAYLOAD_SIZE) {
    LOG_ERROR("CACHE",
              "CacheManager::read_one: Invalid record length %u (Max: %u). Discarding corrupted record.",
              record_len,
              MAX_PAYLOAD_SIZE);
    (void)pop_one();
    return CacheReadError::CORRUPT_DATA;
  }

  if (record_len > buffer_size) {
    LOG_ERROR(
        "CACHE", "CacheManager::read_one: Provided buffer is too small. Needed %u, have %zu", record_len, buffer_size);
    return CacheReadError::OUT_OF_MEMORY;
  }

  uint32_t payload_offset = cacheHeader.tail + sizeof(RECORD_MAGIC) + sizeof(record_len);
  size_t bytes_read = readWithWrap(m_file, payload_offset, (uint8_t*)out_buffer, record_len);

  if (bytes_read != record_len) {
    LOG_ERROR(
        "CACHE", F("CacheManager::read_one: Mismatch in bytes read. Expected %u, got %zu"), record_len, bytes_read);
    return CacheReadError::FILE_READ_ERROR;
  }

  uint32_t stored_crc;
  uint32_t crc_offset = payload_offset + record_len;
  if (readWithWrap(m_file, crc_offset, (uint8_t*)&stored_crc, sizeof(stored_crc)) != sizeof(stored_crc)) {
    LOG_ERROR("CACHE", "CacheManager::read_one: Failed to read stored CRC.");
    return CacheReadError::FILE_READ_ERROR;
  }

  uint32_t calculated_crc = calculate_crc32((const uint8_t*)out_buffer, record_len);
  if (calculated_crc != stored_crc) {
    LOG_ERROR(
        "CACHE", "CRC mismatch! Data corrupted. Stored: 0x%08X, Calc: 0x%08X. Discarding.", stored_crc, calculated_crc);
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
    LOG_ERROR("CACHE", "Pop: Physical Read Error. Skipping 1 byte.");
    advanceTailPointer(1);
    m_dirty = true;
    return true;
  }

  if (magic != RECORD_MAGIC) {
    LOG_WARN("CACHE", "Pop: Sync Loss. Resyncing...");
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
    LOG_ERROR("CACHE", "Pop: Len Read Fail. Skipping 1 byte.");
    advanceTailPointer(1);
    m_dirty = true;
    return true;
  }

  if (record_len > MAX_PAYLOAD_SIZE + RECORD_LEN_TOLERANCE) {
    // If Length is bad, we treat it as Sync failure and advance 1 byte (scan)
    // But since we are "Popping", effectively we just advance 1 byte and return true (item removed/skipped).
    // Wait, if we advance 1 byte, we might find a valid record next call.
    LOG_WARN("CACHE", "Pop: Bad Len %u. Skipping byte.", record_len);
    advanceTailPointer(1);
    m_dirty = true;
    return true;
  }

  uint32_t total_record_size = REDACTED
  advanceTailPointer(total_record_size);

  // POWER SAFETY: Write header but DELAY flushing.
  // if (writeCacheHeader(m_file)) {
  // m_file.flush();
  // }
  m_dirty = true;
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
