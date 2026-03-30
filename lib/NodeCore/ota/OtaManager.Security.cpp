#include "REDACTED"

#include <Arduino.h>
#include <WiFiClientSecureBearSSL.h>

#include <new>

#include "system/ConfigManager.h"
#include "support/CryptoUtils.h"
#include "system/Logger.h"
#include "system/MemoryTelemetry.h"
#include "REDACTED"
#include "generated/root_ca_data.h"

bool OtaManager:REDACTED
  if (m_trustAnchors) {
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

const BearSSL::X509List* OtaManager::activeTrustAnchors() const {
  if (m_trustAnchors) {
    return m_trustAnchors;
  }
  return m_resources.localTrustAnchors.get();
}

bool OtaManager:REDACTED
  m_wifiManager.releaseScanCache();
  CryptoUtils::releaseMainCipherScratch();
  CryptoUtils::releaseWsCipher();
  yield();

  OtaManagerHealth:REDACTED
  const OtaManagerHealth:REDACTED
  if (!tlsBudget.healthy) {
    LOG_WARN("MEM", F("OTA TLS skipped (low heap: REDACTED
    return false;
  }

  auto secure_guard_failed = [&]() {
    return !OtaManagerHealth:REDACTED
                m_policy, m_policy.tlsSecureExtraBlock, m_policy.tlsSecureExtraTotal)
                .healthy;
  };

  auto configure_insecure = [&](bool logFallback) {
    if (logFallback && !m_resources.tlsFallbackWarned) {
      const OtaManagerHealth:REDACTED
          OtaManagerHealth:REDACTED
      LOG_WARN("REDACTED",
               F("OTA TLS fallback to insecure (heap=REDACTED
               secureBudget.freeHeap,
               secureBudget.maxBlock,
               secureBudget.minTotal,
               secureBudget.minBlock);
      m_resources.tlsFallbackWarned = true;
    }
    m_secureClient.stop();
    m_secureClient.setTimeout(m_policy.secureClientTimeoutMs);
    m_secureClient.setTrustAnchors(nullptr);
    m_resources.localTrustAnchors.reset();
    m_secureClient.setInsecure();
    m_secureClient.setBufferSizes(AppConstants::TLS_RX_BUF_SIZE, AppConstants::TLS_TX_BUF_SIZE);
    m_resources.tlsActive = true;
    m_resources.tlsInsecure = true;
    return true;
  };

  if (m_resources.tlsActive) {
    if (!m_resources.tlsInsecure && secure_guard_failed()) {
      return configure_insecure(true);
    }
    return true;
  }

  m_secureClient.stop();
  m_secureClient.setBufferSizes(AppConstants::TLS_RX_BUF_SIZE, AppConstants::TLS_TX_BUF_SIZE);
  m_secureClient.setTimeout(m_policy.secureClientTimeoutMs);

  if (allowInsecure || m_configManager.getConfig().ALLOW_INSECURE_HTTPS()) {
    return configure_insecure(false);
  }

  if (!secure_guard_failed() && ensureTrustAnchors()) {
    const BearSSL::X509List* anchors = activeTrustAnchors();
    if (anchors && !secure_guard_failed()) {
      m_secureClient.setTrustAnchors(anchors);
      m_resources.tlsActive = true;
      m_resources.tlsInsecure = false;
      return true;
    }
  }

  return configure_insecure(true);
}

void OtaManager:REDACTED
  if (!m_resources.tlsActive) {
    return;
  }
  const MemoryTelemetry::HeapSnapshot before = MemoryTelemetry::HeapSnapshot::capture();
  m_secureClient.stop();
  m_secureClient.setTrustAnchors(nullptr);
  m_secureClient.setInsecure();
  m_secureClient.setBufferSizes(AppConstants::TLS_RX_BUF_PORTAL, AppConstants::TLS_TX_BUF_PORTAL);
  m_resources.localTrustAnchors.reset();
  yield();
  const MemoryTelemetry::HeapSnapshot after = MemoryTelemetry::HeapSnapshot::capture();
  MemoryTelemetry::logReleaseSummary("MEM", "OTA TLS release", before, after);
  m_resources.tlsActive = false;
  m_resources.tlsInsecure = false;
  m_resources.tlsFallbackWarned = false;
  OtaManagerHealth:REDACTED
}
