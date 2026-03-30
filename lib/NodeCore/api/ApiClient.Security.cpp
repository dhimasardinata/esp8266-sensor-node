#include "api/ApiClient.h"

#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecureBearSSL.h>
#include <user_interface.h>

#include <memory>

#include "support/CryptoUtils.h"
#include "system/Logger.h"
#include "api/ApiClient.Health.h"
#include "system/MemoryTelemetry.h"
#include "REDACTED"
#include "config/constants.h"
#include "generated/root_ca_data.h"

bool ApiClient::ensureTrustAnchors() {
  if (m_deps.trustAnchors) {
    return true;
  }
  if (m_resources.localTrustAnchors) {
    return true;
  }
  std::unique_ptr<BearSSL::X509List> anchors(new (std::nothrow) BearSSL::X509List(ROOT_CA_PEM));
  if (!anchors) {
    LOG_WARN("MEM", F("Trust anchors alloc failed"));
    return false;
  }
  m_resources.localTrustAnchors.swap(anchors);
  return true;
}

const BearSSL::X509List* ApiClient::activeTrustAnchors() const {
  if (m_deps.trustAnchors) {
    return m_deps.trustAnchors;
  }
  return m_resources.localTrustAnchors.get();
}

void ApiClient::prepareTlsHeap() {
  ApiClientHealth::refreshRuntimeHealth(m_context);
  const ApiClientHealth::HeapBudget budget = ApiClientHealth::captureTlsHeapBudget(m_context);
  const bool tlsPressure = !budget.healthy;
  if (!m_qos.active) {
    m_qos.buffers.reset();
    if (tlsPressure) {
      m_transport.httpClient.reset();
    }
  }
  m_deps.wifiManager.releaseScanCache();
  if (tlsPressure) {
    CryptoUtils::releaseMainCipherScratch();
    CryptoUtils::releaseWsCipher();
    LOG_INFO("MEM",
             F("Released crypto scratch under TLS pressure (free=%u, block=%u, need=%u/%u)"),
             budget.freeHeap,
             budget.maxBlock,
             budget.minTotal,
             budget.minBlock);
  }
}

bool ApiClient::acquireTlsResources(bool allowInsecure) {
  prepareTlsHeap();
  yield();

  const ApiClientHealth::HeapBudget budget = ApiClientHealth::captureTlsHeapBudget(m_context);
  if (!budget.healthy) {
    LOG_WARN("MEM",
             F("TLS alloc skipped (low heap: %u, block %u, need %u/%u)"),
             budget.freeHeap,
             budget.maxBlock,
             budget.minTotal,
             budget.minBlock);
    return false;
  }

  auto secureGuardFailed = [&]() {
    return !ApiClientHealth::captureTlsHeapBudget(m_context, m_policy.tlsSecureExtraBlock, m_policy.tlsSecureExtraTotal).healthy;
  };

  auto configureInsecure = [&](bool logFallback) {
    if (logFallback && !m_resources.tlsFallbackWarned) {
      const ApiClientHealth::HeapBudget secureBudget =
          ApiClientHealth::captureTlsHeapBudget(m_context, m_policy.tlsSecureExtraBlock, m_policy.tlsSecureExtraTotal);
      LOG_WARN("SEC",
               F("API TLS fallback to insecure (heap=%u, block=%u, need=%u/%u)"),
               secureBudget.freeHeap,
               secureBudget.maxBlock,
               secureBudget.minTotal,
               secureBudget.minBlock);
      broadcastEncrypted(F("[SEC] API TLS fallback to insecure (low heap/frag)"));
      m_resources.tlsFallbackWarned = true;
    }
    m_deps.secureClient.stop();
    m_deps.secureClient.setTimeout(m_policy.secureClientTimeoutMs);
    m_deps.secureClient.setTrustAnchors(nullptr);
    m_deps.secureClient.setInsecure();
    m_deps.secureClient.setBufferSizes(AppConstants::TLS_RX_BUF_SIZE, AppConstants::TLS_TX_BUF_SIZE);
    m_resources.tlsActive = true;
    m_resources.tlsInsecure = true;
    return true;
  };

  if (m_resources.tlsActive) {
    if (!m_resources.tlsInsecure && secureGuardFailed()) {
      return configureInsecure(true);
    }
    return true;
  }

  m_deps.secureClient.stop();
  m_deps.secureClient.setTimeout(m_policy.secureClientTimeoutMs);

  if (allowInsecure || m_deps.configManager.getConfig().ALLOW_INSECURE_HTTPS()) {
    return configureInsecure(false);
  }

  if (!secureGuardFailed() && ensureTrustAnchors()) {
    const BearSSL::X509List* anchors = activeTrustAnchors();
    if (anchors && !secureGuardFailed()) {
      m_deps.secureClient.setTrustAnchors(anchors);
      m_deps.secureClient.setBufferSizes(AppConstants::TLS_RX_BUF_SIZE, AppConstants::TLS_TX_BUF_SIZE);
      if (!secureGuardFailed()) {
        m_resources.tlsActive = true;
        m_resources.tlsInsecure = false;
        return true;
      }
    }
  }

  return configureInsecure(true);
}

void ApiClient::releaseTlsResources() {
  if (!m_resources.tlsActive) {
    return;
  }
  const MemoryTelemetry::HeapSnapshot before = MemoryTelemetry::HeapSnapshot::capture();
  m_deps.secureClient.stop();
  m_deps.secureClient.setTrustAnchors(nullptr);
  m_deps.secureClient.setInsecure();
  m_deps.secureClient.setBufferSizes(AppConstants::TLS_RX_BUF_PORTAL, AppConstants::TLS_TX_BUF_PORTAL);
  m_resources.localTrustAnchors.reset();
  yield();
  const MemoryTelemetry::HeapSnapshot after = MemoryTelemetry::HeapSnapshot::capture();
  MemoryTelemetry::logReleaseSummary("MEM", "API TLS release", before, after);
  m_resources.tlsActive = false;
  m_resources.tlsInsecure = false;
  m_resources.tlsFallbackWarned = false;
}

void ApiClient::setTrustAnchors(const BearSSL::X509List* trustAnchors) {
  m_deps.trustAnchors = trustAnchors;
  if (trustAnchors) {
    m_resources.localTrustAnchors.reset();
  }
}
