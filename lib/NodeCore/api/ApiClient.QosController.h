#ifndef API_CLIENT_QOS_CONTROLLER_H
#define API_CLIENT_QOS_CONTROLLER_H

#include "api/ApiClient.h"

class ApiClientQosController {
public:
  using DependencyRefs = ApiClient::DependencyRefs;
  using OperationalState = ApiClient::OperationalState;
  using TransportRuntime = ApiClient::TransportRuntime;
  using HttpState = ApiClient::HttpState;
  using QosTaskType = ApiClient::QosTaskType;
  using QosBuffers = ApiClient::QosBuffers;
  using QosRuntime = ApiClient::QosRuntime;
  using ResourceState = ApiClient::ResourceState;
  using GuardPolicy = ApiClient::GuardPolicy;
  using RuntimeHealth = ApiClient::RuntimeHealth;
  using ControllerContext = ApiClient::ControllerContext;

  explicit ApiClientQosController(ApiClient& api)
      : m_api(api),
        m_ctx(api.m_context),
        m_deps(m_ctx.deps),
        m_runtime(m_ctx.runtime),
        m_transport(m_ctx.transport),
        m_qos(m_ctx.qos),
        m_resources(m_ctx.resources),
        m_policy(m_ctx.policy),
        m_health(m_ctx.health) {}

  void requestUpload();
  void requestOta();
  void handlePendingTask();
  void updateStats(unsigned long duration,
                   int& successCount,
                   unsigned long& totalDuration,
                   unsigned long& minLat,
                   unsigned long& maxLat);
  void reportResults(const char* targetName,
                     int successCount,
                     int samples,
                     unsigned long minLat,
                     unsigned long maxLat,
                     unsigned long totalDuration);
  void performTest(const char* targetName, const char* url, const char* method, const char* payload);

private:
  ApiClient& m_api;
  ControllerContext& m_ctx;
  DependencyRefs& m_deps;
  OperationalState& m_runtime;
  TransportRuntime& m_transport;
  QosRuntime& m_qos;
  ResourceState& m_resources;
  GuardPolicy& m_policy;
  RuntimeHealth& m_health;
};

#endif
