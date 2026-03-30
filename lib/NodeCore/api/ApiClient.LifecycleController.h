#ifndef API_CLIENT_LIFECYCLE_CONTROLLER_H
#define API_CLIENT_LIFECYCLE_CONTROLLER_H

#include "api/ApiClient.h"

class ApiClientLifecycleController {
public:
  explicit ApiClientLifecycleController(ApiClient& api)
      : m_api(api),
        m_ctx(api.m_context),
        m_deps(m_ctx.deps),
        m_runtime(m_ctx.runtime),
        m_transport(m_ctx.transport),
        m_qos(m_ctx.qos),
        m_resources(m_ctx.resources),
        m_policy(m_ctx.policy),
        m_health(m_ctx.health) {}

  void init();
  void applyConfig(const AppConfig& config);
  void handleTimerTasks();
  void checkSoftwareWdt();
  void handle();
  void scheduleImmediateUpload();
  void requestImmediateUpload(bool restoreWsAfterUpload);

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
