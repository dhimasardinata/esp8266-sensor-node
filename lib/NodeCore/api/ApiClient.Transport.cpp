#include "api/ApiClient.h"

#include "net/NtpClient.h"
#include "api/ApiClient.TransportController.h"
#include "api/ApiClient.TransportShared.h"

using namespace ApiClientTransportShared;

bool ApiClient::shouldUseRelayForCloudUpload() const {
  switch (m_runtime.route.uplinkMode) {
    case UplinkMode::DIRECT:
      return false;
    case UplinkMode::RELAY:
      return true;
    case UplinkMode::AUTO:
    default:
      if (m_runtime.route.forceRelayNextCloudAttempt) {
        return true;
      }
      if (m_runtime.route.relayPinnedUntil == 0) {
        return false;
      }
      return static_cast<int32_t>(millis() - m_runtime.route.relayPinnedUntil) < 0;
  }
}

bool ApiClient::shouldTreatAsCloudFailure(const UploadResult& result) const {
  if (result.httpCode < 0) {
    return result.httpCode != HTTPC_ERROR_TOO_LESS_RAM;
  }
  if (result.httpCode == 400 || result.httpCode == 401 || result.httpCode == 422) {
    return false;
  }
  if (result.httpCode >= 300 && result.httpCode < 400) {
    return true;
  }
  if (result.httpCode == 403 || result.httpCode == 408 || result.httpCode == 425 || result.httpCode == 429) {
    return true;
  }
  return result.httpCode >= 500;
}

bool ApiClient::shouldFallbackToRelay(const UploadResult& result) const {
  return m_runtime.route.uplinkMode == UplinkMode::AUTO &&
         !m_runtime.route.targetIsEdge &&
         !m_runtime.route.cloudTargetIsRelay &&
         shouldTreatAsCloudFailure(result);
}

void ApiClient::updateResult(int code, bool success, const char* msg) {
  m_transport.lastResult.httpCode = code;
  m_transport.lastResult.success = success;
  copy_trunc(m_transport.lastResult.message, sizeof(m_transport.lastResult.message), msg);
}

void ApiClient::updateResult_P(int code, bool success, PGM_P msg) {
  m_transport.lastResult.httpCode = code;
  m_transport.lastResult.success = success;
  copy_trunc_P(m_transport.lastResult.message, sizeof(m_transport.lastResult.message), msg);
}

PGM_P ApiClient::uploadSourceLabelP(UploadRecordSource source) {
  switch (source) {
    case UploadRecordSource::RTC:
      return PSTR("RTC");
    case UploadRecordSource::LITTLEFS:
      return PSTR("LittleFS");
    default:
      return PSTR("Unknown");
  }
}

void ApiClient::copyUploadSourceLabel(char* out, size_t out_len, UploadRecordSource source) {
  copy_trunc_P(out, out_len, uploadSourceLabelP(source));
}

void ApiClient::transitionState(HttpState newState) {
  m_transport.httpState = newState;
  m_transport.stateEntryTime = millis();
}

void ApiClient::tryNtpFallbackProbe() {
  ApiClientTransportController(*this).tryNtpFallbackProbe();
}

bool ApiClient::probeServerTimeHeader(bool allowInsecure) {
  return ApiClientTransportController(*this).probeServerTimeHeader(allowInsecure);
}

void ApiClient::updateCloudTargetCache() {
  ApiClientTransportController(*this).updateCloudTargetCache();
}

void ApiClient::updateCloudTargetCacheFor(bool useRelay) {
  ApiClientTransportController(*this).updateCloudTargetCacheFor(useRelay);
}

void ApiClient::activateRelayFallback() {
  ApiClientTransportController(*this).activateRelayFallback();
}

void ApiClient::clearRelayFallback() {
  ApiClientTransportController(*this).clearRelayFallback();
}

void ApiClient::buildErrorMessage(UploadResult& result) {
  ApiClientTransportController(*this).buildErrorMessage(result);
}

void ApiClient::signPayload(const char* payload, size_t payload_len, char* signatureBuffer) {
  ApiClientTransportController(*this).signPayload(payload, payload_len, signatureBuffer);
}

bool ApiClient::executeQosSample(HTTPClient& http,
                                 const char* url,
                                 const char* method,
                                 const char* payload,
                                 bool useOtaToken,
                                 const AppConfig& cfg,
                                 unsigned long& duration) {
  return ApiClientTransportController(*this).executeQosSample(
      http, url, method, payload, useOtaToken, cfg, duration);
}

void ApiClient::startUpload(const char* payload, size_t length, bool isEdgeTarget) {
  ApiClientTransportController(*this).startUpload(payload, length, isEdgeTarget);
}

void ApiClient::handleStateConnecting(const AppConfig& cfg) {
  ApiClientTransportController(*this).handleStateConnecting(cfg);
}

void ApiClient::handleStateSending(const AppConfig& cfg) {
  ApiClientTransportController(*this).handleStateSending(cfg);
}

void ApiClient::handleStateWaiting(unsigned long stateDuration) {
  ApiClientTransportController(*this).handleStateWaiting(stateDuration);
}

void ApiClient::handleStateReading() {
  ApiClientTransportController(*this).handleStateReading();
}

void ApiClient::handleUploadStateMachine() {
  ApiClientTransportController(*this).handleUploadStateMachine();
}

UploadResult ApiClient::performSingleUpload(const char* payload, size_t length, bool allowInsecure) {
  return ApiClientTransportController(*this).performSingleUpload(payload, length, allowInsecure);
}
