#include "api/ApiClient.h"

#include "api/ApiClient.UploadRuntimeController.h"

void ApiClient::notifyLowMemory(uint32_t maxBlock, uint32_t totalFree) {
  ApiClientUploadRuntimeController(*this).notifyLowMemory(maxBlock, totalFree);
}

unsigned long ApiClient::calculateBackoffInterval(const AppConfig& cfg) {
  return ApiClientUploadRuntimeController(*this).calculateBackoffInterval(cfg);
}

void ApiClient::trackUploadFailure() {
  ApiClientUploadRuntimeController(*this).trackUploadFailure();
}

void ApiClient::handleSuccessfulUpload(UploadResult& res, const AppConfig& cfg) {
  ApiClientUploadRuntimeController(*this).handleSuccessfulUpload(res, cfg);
}

void ApiClient::handleFailedUpload(UploadResult& res, const AppConfig& cfg) {
  ApiClientUploadRuntimeController(*this).handleFailedUpload(res, cfg);
}

bool ApiClient::isHeapHealthy() {
  return ApiClientUploadRuntimeController(*this).isHeapHealthy();
}

void ApiClient::processGatewayResult(const UploadResult& res) {
  ApiClientUploadRuntimeController(*this).processGatewayResult(res);
}

int ApiClient::checkGatewayMode() {
  return ApiClientUploadRuntimeController(*this).checkGatewayMode();
}

size_t ApiClient::prepareEdgePayload(size_t rawLen) {
  return ApiClientUploadRuntimeController(*this).prepareEdgePayload(rawLen);
}

void ApiClient::handleUploadCycle() {
  ApiClientUploadRuntimeController(*this).handleUploadCycle();
}

bool ApiClient::dispatchQueuedUploadRecord(size_t record_len, bool isTargetEdge) {
  return ApiClientUploadRuntimeController(*this).dispatchQueuedUploadRecord(record_len, isTargetEdge);
}

bool ApiClient::trySendLiveSnapshotToGateway() {
  return ApiClientUploadRuntimeController(*this).trySendLiveSnapshotToGateway();
}
