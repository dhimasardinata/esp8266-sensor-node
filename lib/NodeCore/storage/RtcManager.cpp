#include "storage/RtcManager.h"

#include <algorithm>
#include <stddef.h>
#include <string.h>

#include "support/Crc32.h"
#include "system/Logger.h"

namespace {

struct alignas(4) LegacyRtcSensorDataV1 {
  uint32_t magic;
  uint16_t head;
  uint16_t tail;
  uint16_t count;
  uint16_t padding;
  RtcSensorRecord records[29];
  uint32_t crc;
};

static_assert(sizeof(LegacyRtcSensorDataV1) == 364, "Legacy RTC V1 layout must remain 364 bytes.");

bool seqBefore(uint16_t a, uint16_t b) {
  if (a == b) return false;
  return static_cast<uint16_t>(b - a) < 0x8000;
}

}  // namespace

RtcSensorData RtcManager::data;

void RtcManager::resetDataInMemory() {
  memset(&data, 0, sizeof(data));
  data.header.blockMagic = RTC_SENSOR_MAGIC;
  data.header.version = RTC_LAYOUT_VERSION;
  data.header.maxRecords = RTC_MAX_RECORDS;
  data.header.head = 0;
  data.header.tail = 0;
  data.header.count = 0;
  data.header.nextSeq = 0;
  data.header.headerCrc = calculateHeaderCrc(data.header);
}

uint32_t RtcManager::calculateHeaderCrc(const RtcHeaderV2& header) {
  return Crc32::compute(&header, offsetof(RtcHeaderV2, headerCrc));
}

uint32_t RtcManager::calculateRecordCrc(const RtcRecordV2& record) {
  return Crc32::compute(&record, offsetof(RtcRecordV2, crc));
}

bool RtcManager::validateHeader(const RtcHeaderV2& header) {
  if (header.blockMagic != RTC_SENSOR_MAGIC) {
    return false;
  }
  if (header.version != RTC_LAYOUT_VERSION) {
    return false;
  }
  if (header.maxRecords != RTC_MAX_RECORDS) {
    return false;
  }
  if (header.count > RTC_MAX_RECORDS || header.head >= RTC_MAX_RECORDS || header.tail >= RTC_MAX_RECORDS) {
    return false;
  }
  return (header.headerCrc == calculateHeaderCrc(header));
}

bool RtcManager::isRecordValid(const RtcRecordV2& record) {
  if (record.magic != RTC_RECORD_MAGIC) {
    return false;
  }
  return (record.crc == calculateRecordCrc(record));
}

void RtcManager::setSlotFromPayload(RtcRecordV2& slot, const RtcSensorRecord& payload, uint16_t seq) {
  memset(&slot, 0, sizeof(slot));
  slot.magic = RTC_RECORD_MAGIC;
  slot.seq = seq;
  slot.timestamp = payload.timestamp;
  slot.temp10 = payload.temp10;
  slot.hum10 = payload.hum10;
  slot.lux = payload.lux;
  slot.rssi = payload.rssi;
  slot.crc = calculateRecordCrc(slot);
}

void RtcManager::payloadFromSlot(RtcSensorRecord& outRecord, const RtcRecordV2& slot) {
  outRecord.timestamp = slot.timestamp;
  outRecord.temp10 = slot.temp10;
  outRecord.hum10 = slot.hum10;
  outRecord.lux = slot.lux;
  outRecord.rssi = slot.rssi;
}

bool RtcManager::readRaw() {
  if (!system_rtc_mem_read(RTC_SENSOR_BLOCK_OFFSET, &data, sizeof(data))) {
    LOG_WARN("RTC", F("RTC read failed"));
    return false;
  }
  return true;
}

bool RtcManager::writeRaw() {
  if (!system_rtc_mem_write(RTC_SENSOR_BLOCK_OFFSET, &data, sizeof(data))) {
    LOG_ERROR("RTC", F("RTC write failed"));
    return false;
  }
  return true;
}

bool RtcManager::writeData() {
  data.header.headerCrc = calculateHeaderCrc(data.header);
  return writeRaw();
}

bool RtcManager::legacyCrcValid(const void* legacyData) {
  if (!legacyData) return false;
  const LegacyRtcSensorDataV1* v1 = static_cast<const LegacyRtcSensorDataV1*>(legacyData);
  uint32_t crc = 0;
  crc = Crc32::compute(&v1->magic, sizeof(v1->magic), crc);
  crc = Crc32::compute(&v1->head, sizeof(v1->head), crc);
  crc = Crc32::compute(&v1->tail, sizeof(v1->tail), crc);
  crc = Crc32::compute(&v1->count, sizeof(v1->count), crc);
  crc = Crc32::compute(&v1->padding, sizeof(v1->padding), crc);
  crc = Crc32::compute(v1->records, sizeof(v1->records), crc);
  return (crc == v1->crc);
}

bool RtcManager::tryMigrateFromLegacy() {
  const LegacyRtcSensorDataV1* oldData = reinterpret_cast<const LegacyRtcSensorDataV1*>(&data);
  if (oldData->magic != RTC_SENSOR_MAGIC) {
    return false;
  }
  if (oldData->count > 29 || oldData->head >= 29 || oldData->tail >= 29) {
    return false;
  }
  if (!legacyCrcValid(oldData)) {
    LOG_WARN("RTC", F("Legacy RTC block detected but CRC invalid"));
    return false;
  }

  const uint16_t available = static_cast<uint16_t>(std::min<uint16_t>(oldData->count, 29));
  const uint16_t importCount = static_cast<uint16_t>(std::min<uint16_t>(available, RTC_MAX_RECORDS));
  const uint16_t startOffset = static_cast<uint16_t>(available > importCount ? available - importCount : 0);

  resetDataInMemory();
  for (uint16_t i = 0; i < importCount; ++i) {
    const uint16_t legacyIndex = static_cast<uint16_t>((oldData->tail + startOffset + i) % 29);
    setSlotFromPayload(data.records[i], oldData->records[legacyIndex], i);
  }
  data.header.tail = 0;
  data.header.count = importCount;
  data.header.head = static_cast<uint16_t>(importCount % RTC_MAX_RECORDS);
  data.header.nextSeq = importCount;

  if (!writeData()) {
    return false;
  }

  if (available > importCount) {
    LOG_WARN("RTC",
             F("Legacy migration truncated %u -> %u records due RTC V2 capacity"),
             static_cast<unsigned>(available),
             static_cast<unsigned>(importCount));
  } else {
    LOG_INFO("RTC", F("Legacy RTC migrated: %u records"), static_cast<unsigned>(importCount));
  }
  return true;
}

bool RtcManager::salvageFromCurrentSlots() {
  struct SlotCopy {
    uint16_t seq;
    RtcSensorRecord payload;
  };

  SlotCopy valid[RTC_MAX_RECORDS];
  uint16_t validCount = 0;

  for (uint16_t i = 0; i < RTC_MAX_RECORDS; ++i) {
    const RtcRecordV2& slot = data.records[i];
    if (!isRecordValid(slot)) {
      continue;
    }
    valid[validCount].seq = slot.seq;
    payloadFromSlot(valid[validCount].payload, slot);
    validCount++;
  }

  if (validCount > 0) {
    std::sort(valid, valid + validCount, [](const SlotCopy& a, const SlotCopy& b) {
      return seqBefore(a.seq, b.seq);
    });

    uint16_t writeIndex = 0;
    for (uint16_t i = 0; i < validCount; ++i) {
      if (writeIndex > 0 && valid[writeIndex - 1].seq == valid[i].seq) {
        continue;
      }
      valid[writeIndex] = valid[i];
      writeIndex++;
    }
    validCount = writeIndex;
  }

  resetDataInMemory();
  const uint16_t count = static_cast<uint16_t>(std::min<uint16_t>(validCount, RTC_MAX_RECORDS));
  for (uint16_t i = 0; i < count; ++i) {
    setSlotFromPayload(data.records[i], valid[i].payload, valid[i].seq);
  }
  data.header.tail = 0;
  data.header.count = count;
  data.header.head = static_cast<uint16_t>(count % RTC_MAX_RECORDS);
  data.header.nextSeq = (count > 0) ? static_cast<uint16_t>(valid[count - 1].seq + 1U) : 0;

  if (!writeData()) {
    return false;
  }

  if (count == 0) {
    LOG_WARN("RTC", F("RTC header corrupt and no valid slots, reset fresh"));
  } else {
    LOG_WARN("RTC", F("RTC header corrupt, salvaged %u slots"), static_cast<unsigned>(count));
  }
  return true;
}

RtcReadStatus RtcManager::sanitizeFrontSlots(uint16_t budgetSlots) {
  if (data.header.count == 0) {
    const bool needNormalizeWrite = (data.header.head != 0 || data.header.tail != 0);
    data.header.head = 0;
    data.header.tail = 0;
    if (needNormalizeWrite && !writeData()) return RtcReadStatus::FILE_READ_ERROR;
    return RtcReadStatus::NONE;
  }

  uint16_t removed = 0;
  while (data.header.count > 0 && removed < budgetSlots) {
    const uint16_t tail = data.header.tail;
    if (isRecordValid(data.records[tail])) {
      break;
    }

    LOG_WARN("RTC",
             F("Dropping corrupt RTC slot idx=%u seq=%u"),
             static_cast<unsigned>(tail),
             static_cast<unsigned>(data.records[tail].seq));
    memset(&data.records[tail], 0, sizeof(RtcRecordV2));
    data.header.tail = static_cast<uint16_t>((tail + 1U) % RTC_MAX_RECORDS);
    data.header.count--;
    removed++;
  }

  if (data.header.count == 0) {
    data.header.head = 0;
    data.header.tail = 0;
  }

  if (removed > 0) {
    if (!writeData()) return RtcReadStatus::FILE_READ_ERROR;
    if (data.header.count > 0 && !isRecordValid(data.records[data.header.tail])) {
      return RtcReadStatus::SCANNING;
    }
    return RtcReadStatus::CORRUPT_DATA;
  }

  if (data.header.count > 0 && !isRecordValid(data.records[data.header.tail])) {
    return RtcReadStatus::SCANNING;
  }

  return RtcReadStatus::NONE;
}

RtcReadStatus RtcManager::loadAndHeal() {
  if (!readRaw()) {
    return RtcReadStatus::FILE_READ_ERROR;
  }

  if (validateHeader(data.header)) {
    return sanitizeFrontSlots(RTC_RECOVERY_BUDGET_SLOTS);
  }

  if (tryMigrateFromLegacy()) {
    return RtcReadStatus::CORRUPT_DATA;
  }

  if (salvageFromCurrentSlots()) {
    return RtcReadStatus::CORRUPT_DATA;
  }

  resetDataInMemory();
  if (!writeData()) {
    return RtcReadStatus::FILE_READ_ERROR;
  }
  return RtcReadStatus::CORRUPT_DATA;
}

void RtcManager::init() {
  RtcReadStatus status = loadAndHeal();
  if (status == RtcReadStatus::FILE_READ_ERROR) {
    LOG_ERROR("RTC", F("RTC init failed: read/write error"));
    return;
  }
  if (status == RtcReadStatus::CORRUPT_DATA || status == RtcReadStatus::SCANNING) {
    LOG_WARN("RTC", F("RTC init required recovery, retrying sanitize"));
    status = loadAndHeal();
    if (status == RtcReadStatus::FILE_READ_ERROR) {
      LOG_ERROR("RTC", F("RTC init failed after recovery"));
      return;
    }
  }
  LOG_INFO("RTC", F("RTC cache ready: count=%u"), static_cast<unsigned>(data.header.count));
}

bool RtcManager::append(uint32_t timestamp, int16_t temp10, int16_t hum10, uint16_t lux, int16_t rssi) {
  RtcReadStatus status = loadAndHeal();
  if (status == RtcReadStatus::FILE_READ_ERROR) {
    return false;
  }

  if (data.header.count >= RTC_MAX_RECORDS) {
    LOG_WARN("RTC", F("RTC full, overwriting oldest slot"));
    data.header.tail = static_cast<uint16_t>((data.header.tail + 1U) % RTC_MAX_RECORDS);
    data.header.count--;
  }

  RtcSensorRecord payload;
  payload.timestamp = timestamp;
  payload.temp10 = temp10;
  payload.hum10 = hum10;
  payload.lux = lux;
  payload.rssi = rssi;

  RtcRecordV2& slot = data.records[data.header.head];
  setSlotFromPayload(slot, payload, data.header.nextSeq);

  data.header.head = static_cast<uint16_t>((data.header.head + 1U) % RTC_MAX_RECORDS);
  data.header.count++;
  data.header.nextSeq = static_cast<uint16_t>(data.header.nextSeq + 1U);

  return writeData();
}

bool RtcManager::isFull() {
  RtcReadStatus status = loadAndHeal();
  if (status == RtcReadStatus::FILE_READ_ERROR) {
    return false;
  }
  return data.header.count >= RTC_MAX_RECORDS;
}

RtcReadStatus RtcManager::peekEx(RtcSensorRecord& outRecord) {
  RtcReadStatus status = loadAndHeal();
  if (status != RtcReadStatus::NONE) {
    return status;
  }
  if (data.header.count == 0) {
    return RtcReadStatus::CACHE_EMPTY;
  }
  const RtcRecordV2& slot = data.records[data.header.tail];
  if (!isRecordValid(slot)) {
    return RtcReadStatus::CORRUPT_DATA;
  }
  payloadFromSlot(outRecord, slot);
  return RtcReadStatus::NONE;
}

RtcReadStatus RtcManager::popEx(RtcSensorRecord& outRecord) {
  RtcReadStatus status = peekEx(outRecord);
  if (status != RtcReadStatus::NONE) {
    return status;
  }

  const uint16_t oldTail = data.header.tail;
  memset(&data.records[oldTail], 0, sizeof(RtcRecordV2));
  data.header.tail = static_cast<uint16_t>((oldTail + 1U) % RTC_MAX_RECORDS);
  data.header.count--;
  if (data.header.count == 0) {
    data.header.head = 0;
    data.header.tail = 0;
  }

  if (!writeData()) {
    return RtcReadStatus::FILE_READ_ERROR;
  }
  return RtcReadStatus::NONE;
}

bool RtcManager::pop(RtcSensorRecord& outRecord) {
  return popEx(outRecord) == RtcReadStatus::NONE;
}

bool RtcManager::peek(RtcSensorRecord& outRecord) {
  return peekEx(outRecord) == RtcReadStatus::NONE;
}

uint16_t RtcManager::getCount() {
  RtcReadStatus status = loadAndHeal();
  if (status == RtcReadStatus::FILE_READ_ERROR) {
    return 0;
  }
  return data.header.count;
}

const RtcSensorData& RtcManager::getRawData() {
  (void)loadAndHeal();
  return data;
}

bool RtcManager::clear() {
  resetDataInMemory();
  return writeData();
}
