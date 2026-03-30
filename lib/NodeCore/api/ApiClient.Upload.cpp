#include "api/ApiClient.UploadController.h"

// ApiClient.Upload.cpp - upload facade wrappers

void ApiClient::buildLocalGatewayUrl(char* buffer, size_t bufferSize) {
  ApiClientUploadController(*this).buildLocalGatewayUrl(buffer, bufferSize);
}

UploadResult ApiClient::performLocalGatewayUpload(const char* payload, size_t length) {
  return ApiClientUploadController(*this).performLocalGatewayUpload(payload, length);
}

void ApiClient::populateEmergencyRecord(EmergencyRecord& outRecord) {
  ApiClientUploadController(*this).populateEmergencyRecord(outRecord);
}

bool ApiClient::buildPayloadFromEmergencyRecord(const EmergencyRecord& record,
                                                char* out,
                                                size_t out_len,
                                                size_t& payload_len) const {
  return ApiClientUploadController(const_cast<ApiClient&>(*this))
      .buildPayloadFromEmergencyRecord(record, out, out_len, payload_len);
}

bool ApiClient::appendEmergencyRecordToRtc(const EmergencyRecord& record, bool announce) {
  return ApiClientUploadController(*this).appendEmergencyRecordToRtc(record, announce);
}

bool ApiClient::appendEmergencyRecordToLittleFs(const EmergencyRecord& record, bool announce) {
  return ApiClientUploadController(*this).appendEmergencyRecordToLittleFs(record, announce);
}

bool ApiClient::tryDirectSendEmergencyRecord(const EmergencyRecord& record, UploadResult& result) {
  return ApiClientUploadController(*this).tryDirectSendEmergencyRecord(record, result);
}

void ApiClient::resetSampleAccumulator() {
  ApiClientUploadController(*this).resetSampleAccumulator();
}

void ApiClient::clearCurrentRecordFlags() {
  ApiClientUploadController(*this).clearCurrentRecordFlags();
}

void ApiClient::clearLoadedRecordContext() {
  ApiClientUploadController(*this).clearLoadedRecordContext();
}

void ApiClient::resetQueuePopRecovery() {
  ApiClientUploadController(*this).resetQueuePopRecovery();
}

void ApiClient::resetRtcFallbackRecovery() {
  ApiClientUploadController(*this).resetRtcFallbackRecovery();
}

void ApiClient::copyCurrentUploadSourceLabel(char* out, size_t out_len) const {
  ApiClientUploadController(const_cast<ApiClient&>(*this)).copyCurrentUploadSourceLabel(out, out_len);
}

void ApiClient::broadcastUploadDispatch(bool immediate, bool isTargetEdge, size_t payload_len) {
  ApiClientUploadController(*this).broadcastUploadDispatch(immediate, isTargetEdge, payload_len);
}

bool ApiClient::finishLoadedRecordSuccess(const AppConfig& cfg, int httpCode, bool setIdleOnPopFailure) {
  return ApiClientUploadController(*this).finishLoadedRecordSuccess(cfg, httpCode, setIdleOnPopFailure);
}

void ApiClient::resetQueuedUploadCycle(bool resetTimer) {
  ApiClientUploadController(*this).resetQueuedUploadCycle(resetTimer);
}

bool ApiClient::recoverPendingQueuePop(const AppConfig& cfg, bool immediate, UploadResult* result) {
  return ApiClientUploadController(*this).recoverPendingQueuePop(cfg, immediate, result);
}

ApiClient::QueuedUploadTargetDecision ApiClient::resolveQueuedUploadTarget(bool& isTargetEdge) {
  return ApiClientUploadController(*this).resolveQueuedUploadTarget(isTargetEdge);
}
