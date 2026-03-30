#ifndef API_CLIENT_QUEUE_CONTROLLER_H
#define API_CLIENT_QUEUE_CONTROLLER_H

#include "api/ApiClient.h"

class ApiClientQueueController {
public:
  explicit ApiClientQueueController(ApiClient& api)
      : m_api(api),
        m_ctx(api.m_context),
        m_deps(m_ctx.deps),
        m_runtime(m_ctx.runtime),
        m_transport(m_ctx.transport),
        m_qos(m_ctx.qos),
        m_resources(m_ctx.resources),
        m_policy(m_ctx.policy),
        m_health(m_ctx.health) {}

  ApiClient::UploadRecordLoad loadRecordFromRtc(size_t& record_len);
  ApiClient::UploadRecordLoad loadRecordFromLittleFs(size_t& record_len);
  ApiClient::UploadRecordLoad loadRecordForUpload(size_t& record_len);
  bool popLoadedRecord();
  void applyQueuePopFailureCooldown(const AppConfig& cfg, const char* sourceTag);
  void logEmergencyQueueState(ApiClient::EmergencyQueueReason reason);
  bool persistEmergencyRecord(const ApiClient::EmergencyRecord& record, bool allowDirectSend);
  void drainEmergencyQueueToStorage(uint8_t maxRecords);
  bool createAndCachePayload();
  void flushRtcToLittleFs();

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
