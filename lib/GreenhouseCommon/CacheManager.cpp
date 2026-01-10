#include "CacheManager.h"
#include "Logger.h"

#include <ConfigManager.h>
#include <FS.h>
#include <LittleFS.h>

#include <memory>

#include "FileGuard.h"
#include "Paths.h"

#define CACHE_MAGIC 0xDEADBEEF

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

static uint32_t crc_table[256];
static bool crc_table_computed = false;

static void make_crc_table(void) {
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (int j = 0; j < 8; j++) {
      c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
    }
    crc_table[i] = c;
  }
  crc_table_computed = true;
}

static uint32_t calculate_crc32(const uint8_t* data, size_t length) {
  if (!crc_table_computed)
    make_crc_table();
  uint32_t crc = 0xFFFFFFFFL;
  for (size_t i = 0; i < length; i++) {
    crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
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
    bytes_read += file.read(buf + space_before_wrap, len - space_before_wrap);
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

// =========================================================================
// == CLASS IMPLEMENTATION
// =========================================================================

CacheManager::CacheManager() {}

void CacheManager::init() {
  File f = LittleFS.open(Paths::CACHE_FILE, "r+");
  if (!f) {
    File f_new = LittleFS.open(Paths::CACHE_FILE, "w+");
    if (!f_new) {
      LOG_ERROR("CACHE", "Failed to create cache file!");
      return;
    }
    FileGuard newCacheFile(f_new);

    cacheHeader.magic = CACHE_MAGIC;
    cacheHeader.version = 3;
    cacheHeader.head = CACHE_DATA_START;
    cacheHeader.tail = CACHE_DATA_START;
    cacheHeader.size = 0;
    if (!writeCacheHeader(newCacheFile)) {
      return;
    }
  } else {
    FileGuard cacheFile(f);

    if (!readCacheHeader(cacheFile) || cacheHeader.magic != CACHE_MAGIC || cacheHeader.version < 3 ||
        calculate_header_crc(cacheHeader) != cacheHeader.crc) {
      LOG_ERROR("CACHE", "Cache header invalid or corrupt. Resetting cache.");
      reset();
      File f_reopen = LittleFS.open(Paths::CACHE_FILE, "r");
      if (f_reopen) {
        FileGuard newCacheFile(f_reopen);
        readCacheHeader(newCacheFile);
      }
      return;

    }
  }
  LOG_INFO("CACHE", "Init OK. Head: %u, Tail: %u, Size: %u bytes",
           cacheHeader.head, cacheHeader.tail, cacheHeader.size);
}

void CacheManager::reset() {
  LOG_WARN("CACHE", "Resetting cache file...");
  LittleFS.remove(Paths::CACHE_FILE);
  init();
}

bool CacheManager::write(const char* data, uint16_t len) {
  if (len == 0)
    return true;

  // 1. Prepare Data & CRC
  uint16_t record_len = len;
  // CRC is critical here: Calculated on RAM data before write
  uint32_t payload_crc = calculate_crc32((const uint8_t*)data, record_len);

  // Total size = Length (2) + Data (N) + CRC (4)
  uint32_t total_len_on_disk = sizeof(record_len) + record_len + sizeof(uint32_t);

  File f = LittleFS.open(Paths::CACHE_FILE, "r+");
  if (!f)
    return false;
  FileGuard cacheFile(f);
  
  // 1.5 Check Filesystem Free Space
  FSInfo fs_info;
  if (LittleFS.info(fs_info)) {
    if (fs_info.totalBytes - fs_info.usedBytes < total_len_on_disk + 1024) { // keep 1KB reserve
        LOG_WARN("CACHE", "Filesystem full! Cannot write.");
        // Optional: Trigger stricter cleanup?
        return false;
    }
  }

  // 2. Ensure Space Exists (Trim Oldest)
  while (cacheHeader.size + total_len_on_disk > MAX_CACHE_DATA_SIZE && cacheHeader.size > 0) {
    if (!pop_one()) {
      LOG_ERROR("CACHE", "Failed to pop old record while trimming.");
      return false;
    }
  }

  if (cacheHeader.size + total_len_on_disk > MAX_CACHE_DATA_SIZE) {
    LOG_ERROR("CACHE", "Record is too large to fit in an empty cache.");
    return false;
  }

  // 3. Write-Verify-Retry Loop
  const int MAX_RETRIES = 3;
  bool writeSuccess = false;

  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    ESP.wdtFeed();  // Prevent watchdog reset during flash operations
    uint32_t write_cursor = cacheHeader.head;

    // --- STEP A: WRITE ---
    // Write Length
    if (writeWithWrap(cacheFile, write_cursor, (const uint8_t*)&record_len, sizeof(record_len)) != sizeof(record_len)) {
        continue; // Retry
    }
    write_cursor += sizeof(record_len);

    // Write Data
    if (writeWithWrap(cacheFile, write_cursor, (const uint8_t*)data, record_len) != record_len) {
        continue; // Retry
    }
    write_cursor += record_len;

    // Write CRC32
    if (writeWithWrap(cacheFile, write_cursor, (const uint8_t*)&payload_crc, sizeof(payload_crc)) != sizeof(payload_crc)) {
        continue; // Retry
    }

    // Force physical write to flash
    cacheFile.flush();

    // --- STEP B: VERIFY ---
    // Check 1: Length
    bool lenOk = verifyWithWrap(cacheFile, cacheHeader.head, (const uint8_t*)&record_len, sizeof(record_len));

    // Check 2: Data
    bool dataOk = verifyWithWrap(cacheFile, cacheHeader.head + sizeof(record_len), (const uint8_t*)data, record_len);

    // Check 3: CRC32
    bool crcOk = verifyWithWrap(cacheFile,
                                cacheHeader.head + sizeof(record_len) + record_len,
                                (const uint8_t*)&payload_crc,
                                sizeof(payload_crc));

    if (lenOk && dataOk && crcOk) {
      writeSuccess = true;
      break;  // Write confirmed good!
    }

    LOG_WARN("CACHE", F("Write verify failed (Attempt %d/%d). Retrying..."), attempt, MAX_RETRIES);
    delay(20);
  }

  if (!writeSuccess) {
    LOG_ERROR("CACHE", "Write Failed. Flash sector likely dead.");
    return false;
  }

  // 4. Update Header (Only if write succeeded)
  uint32_t final_pos = cacheHeader.head + total_len_on_disk;

  // Handle pointer wrapping for the Header state
  if (final_pos >= (CACHE_DATA_START + MAX_CACHE_DATA_SIZE)) {
    cacheHeader.head = CACHE_DATA_START + (final_pos - (CACHE_DATA_START + MAX_CACHE_DATA_SIZE));
  } else {
    cacheHeader.head = final_pos;
  }

  cacheHeader.size += total_len_on_disk;

  bool headerSaved = writeCacheHeader(cacheFile);

  if (headerSaved) {
    LOG_DEBUG("CACHE", "Data saved & verified. New size: %u bytes", cacheHeader.size);
  } else {
    LOG_ERROR("CACHE", "Failed to update cache header after write.");
  }

  return headerSaved;
}

CacheReadError CacheManager::read_one(char* out_buffer, size_t buffer_size, size_t& out_len) {
  out_len = 0;
  if (cacheHeader.size == 0) {
    return CacheReadError::CACHE_EMPTY;
  }

  File f = LittleFS.open(Paths::CACHE_FILE, "r");
  if (!f) {
    LOG_ERROR("CACHE", "CacheManager::read_one: Failed to open cache file.");
    return CacheReadError::FILE_READ_ERROR;
  }
  FileGuard cacheFile(f);

  uint16_t record_len;
  if (readWithWrap(cacheFile, cacheHeader.tail, (uint8_t*)&record_len, sizeof(record_len)) != sizeof(record_len)) {
    LOG_ERROR("CACHE", "CacheManager::read_one: Failed to read record length.");
    return CacheReadError::FILE_READ_ERROR;
  }

  if (record_len == 0 || record_len > MAX_PAYLOAD_SIZE) {
    LOG_ERROR("CACHE", "CacheManager::read_one: Invalid record length %u (Max: %u). Discarding corrupted record.",
             record_len, MAX_PAYLOAD_SIZE);
    pop_one();
    return CacheReadError::CORRUPT_DATA;
  }

  if (record_len > buffer_size) {
    LOG_ERROR("CACHE", "CacheManager::read_one: Provided buffer is too small. Needed %u, have %zu",
             record_len, buffer_size);
    return CacheReadError::OUT_OF_MEMORY;
  }

  uint32_t payload_offset = cacheHeader.tail + sizeof(record_len);
  size_t bytes_read = readWithWrap(cacheFile, payload_offset, (uint8_t*)out_buffer, record_len);

  if (bytes_read != record_len) {
    LOG_ERROR("CACHE", F("CacheManager::read_one: Mismatch in bytes read. Expected %u, got %zu"), record_len, bytes_read);
    return CacheReadError::FILE_READ_ERROR;
  }

  uint32_t stored_crc;
  uint32_t crc_offset = payload_offset + record_len;
  if (readWithWrap(cacheFile, crc_offset, (uint8_t*)&stored_crc, sizeof(stored_crc)) != sizeof(stored_crc)) {
    LOG_ERROR("CACHE", "CacheManager::read_one: Failed to read stored CRC.");
    return CacheReadError::FILE_READ_ERROR;
  }

  uint32_t calculated_crc = calculate_crc32((const uint8_t*)out_buffer, record_len);
  if (calculated_crc != stored_crc) {
    LOG_ERROR("CACHE", "CRC mismatch! Data corrupted. Stored: 0x%08X, Calc: 0x%08X. Discarding.",
             stored_crc, calculated_crc);
    pop_one();
    return CacheReadError::CORRUPT_DATA;
  }

  out_len = record_len;
  return CacheReadError::NONE;
}

bool CacheManager::pop_one() {
  if (cacheHeader.size == 0)
    return true;

  File f = LittleFS.open(Paths::CACHE_FILE, "r+");
  if (!f)
    return false;
  FileGuard cacheFile(f);

  uint16_t record_len;
  if (readWithWrap(cacheFile, cacheHeader.tail, (uint8_t*)&record_len, sizeof(record_len)) != sizeof(record_len)) {
    LOG_ERROR("CACHE", "Cache corruption detected during pop. Resetting.");
    reset();
    return false;
  }

  if (record_len > MAX_PAYLOAD_SIZE + 100) {
    LOG_ERROR("CACHE", "CacheManager::pop_one: Invalid record length %u. Resetting cache.", record_len);
    reset();
    return false;
  }

  uint32_t total_record_size = sizeof(record_len) + record_len + sizeof(uint32_t);
  uint32_t new_tail = cacheHeader.tail + total_record_size;
  if (new_tail >= MAX_CACHE_DATA_SIZE + CACHE_DATA_START) {
    cacheHeader.tail = CACHE_DATA_START + (new_tail - (MAX_CACHE_DATA_SIZE + CACHE_DATA_START));
  } else {
    cacheHeader.tail = new_tail;
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

  return writeCacheHeader(cacheFile);
}

void CacheManager::get_status(uint32_t& size_bytes, uint32_t& head, uint32_t& tail) {
  size_bytes = cacheHeader.size;
  head = cacheHeader.head;
  tail = cacheHeader.tail;
}

uint32_t CacheManager::get_size() {
  return cacheHeader.size;
}