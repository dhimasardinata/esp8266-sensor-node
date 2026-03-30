#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <user_interface.h>

// Public payload shape for caller (without RTC metadata).
struct alignas(4) RtcSensorRecord {
  uint32_t timestamp;
  int16_t temp10;
  int16_t hum10;
  uint16_t lux;
  int16_t rssi;
};

// BootGuard occupies blocks 96..100 (20 bytes), so sensor cache starts at 101.
#define RTC_SENSOR_BLOCK_OFFSET 101
#define RTC_SENSOR_MAGIC 0xCAFEBABE
#define RTC_RECORD_MAGIC 0xBEEF

// Layout V2 uses per-record CRC32 and magic marker.
#define RTC_LAYOUT_VERSION 2
#define RTC_MAX_RECORDS 17
#define RTC_RECOVERY_BUDGET_SLOTS 4

// ESP8266 user RTC memory is blocks [64, 192), each block is 4 bytes.
static constexpr uint16_t RTC_USER_BLOCK_START = 64;
static constexpr uint16_t RTC_USER_BLOCK_END = 192;  // exclusive

enum class RtcReadStatus {
  NONE,
  CACHE_EMPTY,
  FILE_READ_ERROR,
  CORRUPT_DATA,
  SCANNING,
};

struct alignas(4) RtcRecordV2 {
  uint16_t magic;
  uint16_t seq;
  uint32_t timestamp;
  int16_t temp10;
  int16_t hum10;
  uint16_t lux;
  int16_t rssi;
  uint32_t crc;
};

struct alignas(4) RtcHeaderV2 {
  uint32_t blockMagic;
  uint16_t version;
  uint16_t maxRecords;
  uint16_t head;
  uint16_t tail;
  uint16_t count;
  uint16_t nextSeq;
  uint32_t headerCrc;
};

struct alignas(4) RtcSensorData {
  RtcHeaderV2 header;
  RtcRecordV2 records[RTC_MAX_RECORDS];
  uint32_t reserved;
};

static_assert(sizeof(RtcSensorRecord) == 12, "RtcSensorRecord payload must remain 12 bytes.");
static_assert(sizeof(RtcRecordV2) == 20, "RtcRecordV2 layout must remain 20 bytes.");
static_assert(sizeof(RtcHeaderV2) == 20, "RtcHeaderV2 layout must remain 20 bytes.");
static_assert(sizeof(RtcSensorData) == 364, "RtcSensorData must remain 364 bytes in RTC.");
static_assert(sizeof(RtcSensorData) % 4 == 0, "RtcSensorData must be 4-byte aligned for system_rtc_mem_* API");
static_assert((RTC_SENSOR_BLOCK_OFFSET >= RTC_USER_BLOCK_START),
              "RTC_SENSOR_BLOCK_OFFSET must be in ESP8266 RTC user region");
static_assert((RTC_SENSOR_BLOCK_OFFSET + (sizeof(RtcSensorData) / 4)) <= RTC_USER_BLOCK_END,
              "RtcSensorData overflows ESP8266 RTC user region");

class RtcManager {
public:
  static void init();

  // Appends a new sensor record to SRAM caching. Returns true if successful.
  static bool append(uint32_t timestamp, int16_t temp10, int16_t hum10, uint16_t lux, int16_t rssi);

  // Checks if RTC is completely full and requires an immediate LittleFS flush
  static bool isFull();

  // Extended status API.
  static RtcReadStatus peekEx(RtcSensorRecord& outRecord);
  static RtcReadStatus popEx(RtcSensorRecord& outRecord);

  // Pops the oldest sensor record (e.g. on successful cloud sync). Returns true if a record was popped.
  static bool pop(RtcSensorRecord& outRecord);

  // Reads the oldest record without removing it.
  static bool peek(RtcSensorRecord& outRecord);

  // Expose count for debugging/logic
  static uint16_t getCount();

  // Expose the raw data structure directly for CacheManager batch flushing
  static const RtcSensorData& getRawData();

  // Clears all records
  static bool clear();

private:
  static RtcSensorData data;

  static bool readRaw();
  static bool writeRaw();
  static bool writeData();
  static void resetDataInMemory();

  static bool validateHeader(const RtcHeaderV2& header);
  static uint32_t calculateHeaderCrc(const RtcHeaderV2& header);
  static uint32_t calculateRecordCrc(const RtcRecordV2& record);
  static bool isRecordValid(const RtcRecordV2& record);

  static RtcReadStatus loadAndHeal();
  static RtcReadStatus sanitizeFrontSlots(uint16_t budgetSlots);
  static bool salvageFromCurrentSlots();
  static bool tryMigrateFromLegacy();
  static bool legacyCrcValid(const void* legacyData);

  static void setSlotFromPayload(RtcRecordV2& slot, const RtcSensorRecord& payload, uint16_t seq);
  static void payloadFromSlot(RtcSensorRecord& outRecord, const RtcRecordV2& slot);
};

#endif // RTC_MANAGER_H
