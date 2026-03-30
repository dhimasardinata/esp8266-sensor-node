#include "api/ApiClient.h"

#include <new>
#include "system/Logger.h"

// ApiClient.cpp - shared buffer ownership for facade/controllers

bool ApiClient::ensureSharedBuffer() {
  if (m_resources.sharedBuffer) {
    return true;
  }
  std::unique_ptr<PayloadBuffer> buf(new (std::nothrow) PayloadBuffer());
  if (!buf) {
    LOG_WARN("MEM", F("Shared buffer alloc failed"));
    return false;
  }
  (*buf)[0] = '\0';
  m_resources.sharedBuffer.swap(buf);
  return true;
}

void ApiClient::releaseSharedBuffer() {
  m_resources.sharedBuffer.reset();
}

char* ApiClient::sharedBuffer() {
  return m_resources.sharedBuffer ? m_resources.sharedBuffer->data() : nullptr;
}

const char* ApiClient::sharedBuffer() const {
  return m_resources.sharedBuffer ? m_resources.sharedBuffer->data() : nullptr;
}

size_t ApiClient::sharedBufferSize() const {
  return m_resources.sharedBuffer ? m_resources.sharedBuffer->size() : 0;
}
