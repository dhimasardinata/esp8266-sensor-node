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

// --- Additional helpers for complexity reduction ---

static bool hasFilesystemSpace(uint32_t needed_bytes) {
  FSInfo fs_info;
  if (!LittleFS.info(fs_info)) return true; // Assume OK if can't check
  return (fs_info.totalBytes - fs_info.usedBytes >= needed_bytes + 1024);
}

static bool writeRecordData(File& file, uint32_t start_pos, const char* data, 
                             uint16_t record_len, uint32_t payload_crc) {
  uint32_t cursor = start_pos;
  
  if (writeWithWrap(file, cursor, (const uint8_t*)&record_len, sizeof(record_len)) != sizeof(record_len))
    return false;
  cursor += sizeof(record_len);
  
  if (writeWithWrap(file, cursor, (const uint8_t*)data, record_len) != record_len)
    return false;
  cursor += record_len;
  
  if (writeWithWrap(file, cursor, (const uint8_t*)&payload_crc, sizeof(payload_crc)) != sizeof(payload_crc))
    return false;
  
  return true;
}

static bool verifyRecordData(File& file, uint32_t start_pos, const char* data,
                              uint16_t record_len, uint32_t payload_crc) {
  bool lenOk = verifyWithWrap(file, start_pos, (const uint8_t*)&record_len, sizeof(record_len));
  bool dataOk = verifyWithWrap(file, start_pos + sizeof(record_len), (const uint8_t*)data, record_len);
  bool crcOk = verifyWithWrap(file, start_pos + sizeof(record_len) + record_len, 
                               (const uint8_t*)&payload_crc, sizeof(payload_crc));
  return lenOk && dataOk && crcOk;
}

static void updateHeadPointer(uint32_t total_len) {
  uint32_t final_pos = cacheHeader.head + total_len;
  if (final_pos >= (CACHE_DATA_START + MAX_CACHE_DATA_SIZE)) {
    cacheHeader.head = CACHE_DATA_START + (final_pos - (CACHE_DATA_START + MAX_CACHE_DATA_SIZE));
  } else {
    cacheHeader.head = final_pos;
  }
  cacheHeader.size += total_len;
}

static void advanceTailPointer(uint32_t total_record_size) {
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

// =============================================================================
// write() Helper Functions (Reduce Complexity)
// =============================================================================

// Returns true if cache was trimmed successfully, false if failed
// Uses pop_one to avoid code duplication
static bool trimCacheForWrite(uint32_t total_len_on_disk) {
  // Keep popping records until we have space or cache is empty
  while (cacheHeader.size + total_len_on_disk > MAX_CACHE_DATA_SIZE && cacheHeader.size > 0) {
    // Open file to read record length (we need to know size to pop)
    File f = LittleFS.open(Paths::CACHE_FILE, "r+");
    if (!f) return false;
    FileGuard cacheFile(f);
    
    uint16_t record_len;
    if (readWithWrap(cacheFile, cacheHeader.tail, (uint8_t*)&record_len, sizeof(record_len)) != sizeof(record_len)) {
      return false;
    }
    
    if (record_len == 0 || record_len > MAX_PAYLOAD_SIZE) {
      return false;  // Corrupt data
    }
    
    uint32_t total_record_size = sizeof(record_len) + record_len + sizeof(uint32_t);
    advanceTailPointer(total_record_size);
    
    if (!writeCacheHeader(cacheFile)) {
      return false;
    }
  }
  return true;
}

// Returns true if write succeeded with verification
static bool tryWriteWithRetry(FileGuard& cacheFile, const char* data, uint16_t record_len, uint32_t payload_crc) {
  constexpr int MAX_RETRIES = 3;
  
  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    ESP.wdtFeed();
    
    if (!writeRecordData(cacheFile, cacheHeader.head, data, record_len, payload_crc)) {
      continue;
    }
    cacheFile.flush();
    
    if (verifyRecordData(cacheFile, cacheHeader.head, data, record_len, payload_crc)) {
      return true;
    }
    
    LOG_WARN("CACHE", F("Write verify failed (Attempt %d/%d). Retrying..."), attempt, MAX_RETRIES);
    delay(20);
  }
  
  LOG_ERROR("CACHE", "Write Failed. Flash sector likely dead.");
  return false;
}

// =============================================================================
// Main write() - Refactored
// =============================================================================

bool CacheManager::write(const char* data, uint16_t len) {
  if (len == 0) return true;

  uint16_t record_len = len;
  uint32_t payload_crc = calculate_crc32((const uint8_t*)data, record_len);
  uint32_t total_len_on_disk = sizeof(record_len) + record_len + sizeof(uint32_t);

  File f = LittleFS.open(Paths::CACHE_FILE, "r+");
  if (!f) return false;
  FileGuard cacheFile(f);

  // Validation checks
  if (!hasFilesystemSpace(total_len_on_disk)) {
    LOG_WARN("CACHE", "Filesystem full! Cannot write.");
    return false;
  }

  if (!trimCacheForWrite(total_len_on_disk)) {
    LOG_ERROR("CACHE", "Failed to trim cache for write.");
    return false;
  }

  if (cacheHeader.size + total_len_on_disk > MAX_CACHE_DATA_SIZE) {
    LOG_ERROR("CACHE", "Record is too large to fit in cache.");
    return false;
  }

  // Write with retry
  if (!tryWriteWithRetry(cacheFile, data, record_len, payload_crc)) {
    return false;
  }

  updateHeadPointer(total_len_on_disk);

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
  if (cacheHeader.size == 0) return true;

  File f = LittleFS.open(Paths::CACHE_FILE, "r+");
  if (!f) return false;
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
  advanceTailPointer(total_record_size);

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