#include "api/ApiClient.ControlController.h"

#include <ESPAsyncWebServer.h>

#include <array>

#include "system/ConfigManager.h"
#include "support/CryptoUtils.h"
#include "system/Logger.h"

#include "api/ApiClient.CoreShared.h"

// ApiClient.Control.cpp - control/mode facade and websocket broadcast controller

void ApiClientControlController::setUploadMode(UploadMode mode) {
  m_runtime.route.uploadMode = mode;
  m_api.clearLoadedRecordContext();

  switch (mode) {
    case UploadMode::CLOUD:
      m_runtime.route.localGatewayMode = false;
      LOG_INFO("MODE", F("Upload mode set to CLOUD (forced)"));
      break;
    case UploadMode::EDGE:
      m_runtime.route.localGatewayMode = true;
      LOG_INFO("MODE", F("Upload mode set to EDGE (forced)"));
      break;
    case UploadMode::AUTO:
    default:
      LOG_INFO("MODE", F("Upload mode set to AUTO (automatic fallback)"));
      break;
  }

  broadcastEncrypted(m_runtime.route.uploadMode == UploadMode::AUTO    ? F("[MODE] Auto")
                     : m_runtime.route.uploadMode == UploadMode::CLOUD ? F("[MODE] Cloud")
                                                                       : F("[MODE] Edge"));
}

void ApiClientControlController::copyUploadModeString(char* out, size_t out_len) const {
  PGM_P mode = PSTR("auto");
  switch (m_runtime.route.uploadMode) {
    case UploadMode::CLOUD:
      mode = PSTR("cloud");
      break;
    case UploadMode::EDGE:
      mode = PSTR("edge");
      break;
    case UploadMode::AUTO:
    default:
      break;
  }
  copy_trunc_P(out, out_len, mode);
}

void ApiClientControlController::copyUplinkModeString(char* out, size_t out_len) const {
  PGM_P mode = PSTR("auto");
  switch (m_runtime.route.uplinkMode) {
    case UplinkMode::DIRECT:
      mode = PSTR("direct");
      break;
    case UplinkMode::RELAY:
      mode = PSTR("relay");
      break;
    case UplinkMode::AUTO:
    default:
      break;
  }
  copy_trunc_P(out, out_len, mode);
}

void ApiClientControlController::copyActiveCloudRouteString(char* out, size_t out_len) const {
  copy_trunc_P(out, out_len, m_api.shouldUseRelayForCloudUpload() ? PSTR("relay") : PSTR("direct"));
}

void ApiClientControlController::broadcastUploadTarget(bool isEdge) {
  char url[128] = {0};
  if (isEdge) {
    m_api.buildLocalGatewayUrl(url, sizeof(url));
    if (url[0] == '\0') {
      strcpy_P(url, PSTR("gateway"));
    }
  } else {
    m_api.updateCloudTargetCache();
    char fallbackHost[sizeof("example.com")];
    char fallbackPath[sizeof("/api/sensor")];
    char scheme[sizeof("https")];
    copy_trunc_P(fallbackHost, sizeof(fallbackHost), PSTR("example.com"));
    copy_trunc_P(fallbackPath, sizeof(fallbackPath), PSTR("/api/sensor"));
    copy_trunc_P(scheme, sizeof(scheme), PSTR("https"));
    const char* host = (m_transport.cloudHost[0] != '\0') ? m_transport.cloudHost : fallbackHost;
    const char* path = (m_transport.cloudPath[0] != '\0') ? m_transport.cloudPath : fallbackPath;
    const char* raw = m_deps.configManager.getDataUploadUrl();
    if (raw && strncmp_P(raw, PSTR("http://"), 7) == 0) {
      copy_trunc_P(scheme, sizeof(scheme), PSTR("http"));
    }
    size_t pos = 0;
    auto append = [&](const char* text, size_t len) {
      if (!text || len == 0 || pos + 1 >= sizeof(url)) {
        return;
      }
      size_t remaining = sizeof(url) - pos - 1;
      if (len > remaining) {
        len = remaining;
      }
      if (len > 0) {
        memcpy(url + pos, text, len);
        pos += len;
        url[pos] = '\0';
      }
    };
    append(scheme, strnlen(scheme, 8));
    append("://", 3);
    append(host, strnlen(host, sizeof(m_transport.cloudHost)));
    append(path, strnlen(path, sizeof(m_transport.cloudPath)));
  }

  char msg[160] = {0};
  (void)snprintf_P(msg,
                   sizeof(msg),
                   isEdge ? PSTR("[UPLOAD] target=%s (gateway)") : PSTR("[UPLOAD] target=%s (%s)"),
                   url,
                   m_runtime.route.cloudTargetIsRelay ? "relay" : "direct");
  broadcastEncrypted(std::string_view(msg, strnlen(msg, sizeof(msg))));
  m_deps.configManager.releaseStrings();
}

void ApiClientControlController::broadcastEncrypted(const char* text) {
  if (!text) {
    return;
  }
  broadcastEncrypted(std::string_view(text, strlen(text)));
}

void ApiClientControlController::broadcastEncrypted(const __FlashStringHelper* text) {
  if (!text || m_deps.ws.count() == 0) {
    return;
  }
  constexpr size_t kMaxText = CryptoUtils::MAX_PLAINTEXT_SIZE;
  std::array<char, CryptoUtils::ENCRYPTION_BUFFER_SIZE> encScratch{};
  std::array<char, kMaxText> plainScratch{};
  PGM_P textP = reinterpret_cast<PGM_P>(text);
  size_t totalLen = REDACTED
  size_t offset = 0;
  while (offset < totalLen) {
    size_t chunk = totalLen - offset;
    if (chunk > kMaxText) {
      chunk = kMaxText;
    }
    memcpy_P(plainScratch.data(), textP + offset, chunk);
    size_t written =
        CryptoUtils::fast_serialize_encrypted_main(std::string_view(plainScratch.data(), chunk), encScratch.data(), encScratch.size());
    if (written == 0) {
      break;
    }
    m_deps.ws.textAll(encScratch.data(), written);
    offset += chunk;
  }
}

void ApiClientControlController::broadcastEncrypted(std::string_view text) {
  if (text.empty() || m_deps.ws.count() == 0) {
    return;
  }
  constexpr size_t kMaxText = CryptoUtils::MAX_PLAINTEXT_SIZE;
  static_assert(kMaxText <= CryptoUtils::MAX_PLAINTEXT_SIZE, "kMaxText exceeds plaintext limit");
  std::array<char, CryptoUtils::ENCRYPTION_BUFFER_SIZE> encScratch{};
  size_t offset = 0;
  while (offset < text.size()) {
    size_t chunk = text.size() - offset;
    if (chunk > kMaxText) {
      chunk = kMaxText;
    }
    size_t written =
        CryptoUtils::fast_serialize_encrypted_main(text.substr(offset, chunk), encScratch.data(), encScratch.size());
    if (written == 0) {
      break;
    }
    m_deps.ws.textAll(encScratch.data(), written);
    offset += chunk;
  }
}

void ApiClientControlController::pause() {
  if (!m_runtime.isSystemPaused) {
    LOG_INFO("REDACTED", F("REDACTED"));
    m_runtime.isSystemPaused = true;
    m_transport.httpState = ApiClient::HttpState::IDLE;
    m_runtime.uploadState = ApiClient::UploadState::PAUSED;
    if (m_transport.activeClient && m_transport.activeClient->connected()) {
      m_transport.activeClient->stop();
    }
    m_transport.httpClient.reset();
    m_api.releaseTlsResources();
  }
}

void ApiClientControlController::resume() {
  if (m_runtime.isSystemPaused) {
    LOG_INFO("API", F("System Resumed."));
    m_runtime.isSystemPaused = false;
    m_runtime.uploadState = ApiClient::UploadState::IDLE;
    m_runtime.consecutiveUploadFailures = 0;
  }
}

void ApiClientControlController::setOtaInProgress(bool inProgress) {
  m_runtime.otaInProgress = REDACTED
}

bool ApiClientControlController::isUploadActive() const {
  return (m_transport.httpState != ApiClient::HttpState::IDLE) || m_qos.active;
}

void ApiClient::setUploadMode(UploadMode mode) {
  ApiClientControlController(*this).setUploadMode(mode);
}

void ApiClient::copyUploadModeString(char* out, size_t out_len) const {
  ApiClientControlController(const_cast<ApiClient&>(*this)).copyUploadModeString(out, out_len);
}

void ApiClient::copyUplinkModeString(char* out, size_t out_len) const {
  ApiClientControlController(const_cast<ApiClient&>(*this)).copyUplinkModeString(out, out_len);
}

void ApiClient::copyActiveCloudRouteString(char* out, size_t out_len) const {
  ApiClientControlController(const_cast<ApiClient&>(*this)).copyActiveCloudRouteString(out, out_len);
}

const char* ApiClient::gatewayModeLabel(int mode) {
  switch (mode) {
    case 0:
      return "CLOUD";
    case 1:
      return "LOCAL";
    case 2:
      return "AUTO";
    default:
      return "UNKNOWN";
  }
}

void ApiClient::broadcastUploadTarget(bool isEdge) {
  ApiClientControlController(*this).broadcastUploadTarget(isEdge);
}

void ApiClient::broadcastEncrypted(const char* text) {
  ApiClientControlController(*this).broadcastEncrypted(text);
}

void ApiClient::broadcastEncrypted(const __FlashStringHelper* text) {
  ApiClientControlController(*this).broadcastEncrypted(text);
}

void ApiClient::broadcastEncrypted(std::string_view text) {
  ApiClientControlController(*this).broadcastEncrypted(text);
}

void ApiClient::pause() {
  ApiClientControlController(*this).pause();
}

void ApiClient::resume() {
  ApiClientControlController(*this).resume();
}

void ApiClient::setOtaInProgress(bool inProgress) {
  ApiClientControlController(*this).setOtaInProgress(inProgress);
}

bool ApiClient::isUploadActive() const {
  return ApiClientControlController(const_cast<ApiClient&>(*this)).isUploadActive();
}
