#ifndef API_CLIENT_TRANSPORT_CONTROLLER_H
#define API_CLIENT_TRANSPORT_CONTROLLER_H

#include "api/ApiClient.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

class ApiClientTransportController {
public:
  using HttpState = ApiClient::HttpState;
  using DependencyRefs = ApiClient::DependencyRefs;
  using OperationalState = ApiClient::OperationalState;
  using RoutingState = ApiClient::RoutingState;
  using TransportRuntime = ApiClient::TransportRuntime;
  using QosRuntime = ApiClient::QosRuntime;
  using ResourceState = ApiClient::ResourceState;
  using GuardPolicy = ApiClient::GuardPolicy;
  using RuntimeHealth = ApiClient::RuntimeHealth;
  using ControllerContext = ApiClient::ControllerContext;

  explicit ApiClientTransportController(ApiClient& api)
      : m_api(api),
        m_ctx(api.m_context),
        m_deps(m_ctx.deps),
        m_runtime(m_ctx.runtime),
        m_transport(m_ctx.transport),
        m_qos(m_ctx.qos),
        m_resources(m_ctx.resources),
        m_policy(m_ctx.policy),
        m_health(m_ctx.health) {}

  void tryNtpFallbackProbe();
  bool probeServerTimeHeader(bool allowInsecure);
  void updateCloudTargetCache();
  void updateCloudTargetCacheFor(bool useRelay);
  void activateRelayFallback();
  void clearRelayFallback();
  void buildErrorMessage(UploadResult& result);
  void signPayload(const char* payload, size_t payload_len, char* signatureBuffer);
  bool executeQosSample(HTTPClient& http,
                        const char* url,
                        const char* method,
                        const char* payload,
                        bool useOtaToken,
                        const AppConfig& cfg,
                        unsigned long& duration);
  void startUpload(const char* payload, size_t length, bool isEdgeTarget);
  void handleStateConnecting(const AppConfig& cfg);
  void handleStateSending(const AppConfig& cfg);
  void handleStateWaiting(unsigned long stateDuration);
  void handleStateReading();
  void handleUploadStateMachine();
  UploadResult performSingleUpload(const char* payload, size_t length, bool allowInsecure);

private:
  bool acquireTlsResources(bool allowInsecure) {
    return m_api.acquireTlsResources(allowInsecure);
  }

  void releaseTlsResources() {
    m_api.releaseTlsResources();
  }

  bool shouldUseRelayForCloudUpload() const {
    return m_api.shouldUseRelayForCloudUpload();
  }

  bool shouldFallbackToRelay(const UploadResult& result) const {
    return m_api.shouldFallbackToRelay(result);
  }

  void updateResult(int code, bool success, const char* msg) {
    m_api.updateResult(code, success, msg);
  }

  void updateResult_P(int code, bool success, PGM_P msg) {
    m_api.updateResult_P(code, success, msg);
  }

  void transitionState(HttpState newState) {
    m_api.transitionState(newState);
  }

  void broadcastUploadTarget(bool isEdge) {
    m_api.broadcastUploadTarget(isEdge);
  }

  char* sharedBuffer() {
    return m_api.sharedBuffer();
  }

  void releaseSharedBuffer() {
    m_api.releaseSharedBuffer();
  }

  ApiClient& m_api;
  ControllerContext& m_ctx;
  DependencyRefs& m_deps;
  OperationalState& m_runtime;
  TransportRuntime& m_transport;
  QosRuntime& m_qos;
  ResourceState& m_resources;
  GuardPolicy& m_policy;
  RuntimeHealth& m_health;

  static constexpr unsigned long RELAY_FALLBACK_PIN_MS = ApiClient::RELAY_FALLBACK_PIN_MS;
};

#endif
