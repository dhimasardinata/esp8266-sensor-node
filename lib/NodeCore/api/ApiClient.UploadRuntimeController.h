#ifndef API_CLIENT_UPLOAD_RUNTIME_CONTROLLER_H
#define API_CLIENT_UPLOAD_RUNTIME_CONTROLLER_H

#include "api/ApiClient.h"

class ApiClientUploadRuntimeController {
public:
  explicit ApiClientUploadRuntimeController(ApiClient& api)
      : m_api(api),
        m_ctx(api.m_context),
        m_deps(m_ctx.deps),
        m_runtime(m_ctx.runtime),
        m_transport(m_ctx.transport),
        m_qos(m_ctx.qos),
        m_resources(m_ctx.resources),
        m_policy(m_ctx.policy),
        m_health(m_ctx.health) {}

  void notifyLowMemory(uint32_t maxBlock, uint32_t totalFree);
  unsigned long calculateBackoffInterval(const AppConfig& cfg);
  void trackUploadFailure();
  void handleSuccessfulUpload(UploadResult& res, const AppConfig& cfg);
  void handleFailedUpload(UploadResult& res, const AppConfig& cfg);
  bool isHeapHealthy();
  void processGatewayResult(const UploadResult& res);
  int checkGatewayMode();
  size_t prepareEdgePayload(size_t rawLen);
  void handleUploadCycle();
  bool dispatchQueuedUploadRecord(size_t record_len, bool isTargetEdge);
  bool trySendLiveSnapshotToGateway();

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
