#include "api/ApiClient.QueueController.h"

// ApiClient.Queue.cpp - queue facade wrappers and in-memory emergency ring buffer

ApiClient::UploadRecordLoad ApiClient::loadRecordFromRtc(size_t& record_len) {
  return ApiClientQueueController(*this).loadRecordFromRtc(record_len);
}

ApiClient::UploadRecordLoad ApiClient::loadRecordFromLittleFs(size_t& record_len) {
  return ApiClientQueueController(*this).loadRecordFromLittleFs(record_len);
}

ApiClient::UploadRecordLoad ApiClient::loadRecordForUpload(size_t& record_len) {
  return ApiClientQueueController(*this).loadRecordForUpload(record_len);
}

bool ApiClient::popLoadedRecord() {
  return ApiClientQueueController(*this).popLoadedRecord();
}

void ApiClient::applyQueuePopFailureCooldown(const AppConfig& cfg, const char* sourceTag) {
  ApiClientQueueController(*this).applyQueuePopFailureCooldown(cfg, sourceTag);
}

bool ApiClient::enqueueEmergencyRecord(const EmergencyRecord& record) {
  if (m_runtime.queue.emergencyCount >= kEmergencyQueueCapacity) {
    return false;
  }
  m_runtime.queue.emergencyQueue[m_runtime.queue.emergencyHead] = record;
  m_runtime.queue.emergencyHead = static_cast<uint8_t>((m_runtime.queue.emergencyHead + 1U) % kEmergencyQueueCapacity);
  m_runtime.queue.emergencyCount++;
  return true;
}

bool ApiClient::peekEmergencyRecord(EmergencyRecord& out) const {
  if (m_runtime.queue.emergencyCount == 0) {
    return false;
  }
  out = m_runtime.queue.emergencyQueue[m_runtime.queue.emergencyTail];
  return true;
}

bool ApiClient::popEmergencyRecord(EmergencyRecord& out) {
  if (m_runtime.queue.emergencyCount == 0) {
    return false;
  }
  out = m_runtime.queue.emergencyQueue[m_runtime.queue.emergencyTail];
  m_runtime.queue.emergencyTail = static_cast<uint8_t>((m_runtime.queue.emergencyTail + 1U) % kEmergencyQueueCapacity);
  m_runtime.queue.emergencyCount--;
  if (m_runtime.queue.emergencyCount == 0) {
    m_runtime.queue.emergencyHead = 0;
    m_runtime.queue.emergencyTail = 0;
  }
  return true;
}

void ApiClient::logEmergencyQueueState(EmergencyQueueReason reason) {
  ApiClientQueueController(*this).logEmergencyQueueState(reason);
}

bool ApiClient::persistEmergencyRecord(const EmergencyRecord& record, bool allowDirectSend) {
  return ApiClientQueueController(*this).persistEmergencyRecord(record, allowDirectSend);
}

void ApiClient::drainEmergencyQueueToStorage(uint8_t maxRecords) {
  ApiClientQueueController(*this).drainEmergencyQueueToStorage(maxRecords);
}

bool ApiClient::createAndCachePayload() {
  return ApiClientQueueController(*this).createAndCachePayload();
}

void ApiClient::flushRtcToLittleFs() {
  ApiClientQueueController(*this).flushRtcToLittleFs();
}
