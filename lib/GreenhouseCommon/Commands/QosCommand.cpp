#include "QosCommand.h"

#include "ApiClient.h"
#include "utils.h"

QosUploadCommand::QosUploadCommand(ApiClient& apiClient) : m_apiClient(apiClient) {}

void QosUploadCommand::execute(const CommandContext& context) {
  m_apiClient.requestQosUpload();
  Utils::ws_printf(context.client, "QoS Upload Test scheduled. Please wait for results...");
}

QosOtaCommand:REDACTED

void QosOtaCommand:REDACTED
  m_apiClient.requestQosOta();
  Utils::ws_printf(context.client, "QoS OTA Test scheduled. Please wait for results...");
}