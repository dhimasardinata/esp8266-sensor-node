#ifndef API_CLIENT_UPLOAD_CONTROLLER_H
#define API_CLIENT_UPLOAD_CONTROLLER_H

#include "api/ApiClient.h"

class ApiClientUploadController {
public:
  explicit ApiClientUploadController(ApiClient& api)
      : m_api(api),
        m_ctx(api.m_context),
        m_deps(m_ctx.deps),
        m_runtime(m_ctx.runtime),
        m_transport(m_ctx.transport),
        m_qos(m_ctx.qos),
        m_resources(m_ctx.resources),
        m_policy(m_ctx.policy),
        m_health(m_ctx.health) {}

  void buildLocalGatewayUrl(char* buffer, size_t bufferSize);
  UploadResult performLocalGatewayUpload(const char* payload, size_t length);
  void populateEmergencyRecord(ApiClient::EmergencyRecord& outRecord);
  bool buildPayloadFromEmergencyRecord(const ApiClient::EmergencyRecord& record,
                                       char* out,
                                       size_t out_len,
                                       size_t& payload_len) const;
  bool appendEmergencyRecordToRtc(const ApiClient::EmergencyRecord& record, bool announce);
  bool appendEmergencyRecordToLittleFs(const ApiClient::EmergencyRecord& record, bool announce);
  bool tryDirectSendEmergencyRecord(const ApiClient::EmergencyRecord& record, UploadResult& result);
  void resetSampleAccumulator();
  void clearCurrentRecordFlags();
  void clearLoadedRecordContext();
  void resetQueuePopRecovery();
  void resetRtcFallbackRecovery();
  void copyCurrentUploadSourceLabel(char* out, size_t out_len) const;
  void broadcastUploadDispatch(bool immediate, bool isTargetEdge, size_t payload_len);
  bool finishLoadedRecordSuccess(const AppConfig& cfg, int httpCode, bool setIdleOnPopFailure);
  void resetQueuedUploadCycle(bool resetTimer);
  bool recoverPendingQueuePop(const AppConfig& cfg, bool immediate, UploadResult* result);
  ApiClient::QueuedUploadTargetDecision resolveQueuedUploadTarget(bool& isTargetEdge);

private:
  ApiClient& m_api;
  ApiClient::ControllerContext& m_ctx;
  ApiClient::DependencyRefs& m_deps;
  ApiClient::OperationalState& m_runtime;
  ApiClient::TransportRuntime& m_transport;
  ApiClient::QosRuntime& m_qos;
  ApiClient::ResourceState& m_resources;
  ApiClient::GuardPolicy& m_policy;
  ApiClient::RuntimeHealth& m_health;
};

#endif
