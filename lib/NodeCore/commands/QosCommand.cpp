#include "QosCommand.h"

#include "api/ApiClient.h"
#include "support/Utils.h"

QosUploadCommand::QosUploadCommand(ApiClient& apiClient) : m_apiClient(apiClient) {}

void QosUploadCommand::execute(const CommandContext& context) {
  m_apiClient.requestQosUpload();
  Utils::ws_printf_P(context.client, PSTR("QoS Upload Test scheduled. Please wait for results..."));
}

QosOtaCommand:REDACTED

void QosOtaCommand:REDACTED
  m_apiClient.requestQosOta();
  Utils::ws_printf_P(context.client, PSTR("QoS OTA Test scheduled. Please wait for results..."));
}
