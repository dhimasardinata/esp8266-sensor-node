#ifndef QOS_COMMAND_H
#define QOS_COMMAND_H

#include "ICommand.h"

class ApiClient;

class QosUploadCommand : public ICommand {
public:
  explicit QosUploadCommand(ApiClient& apiClient);
  const char* getName() const override {
    return "qos-upload";
  }
  const char* getDescription() const override {
    return "Tests latency and loss to the Data Upload API (Runs in background).";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ApiClient& m_apiClient;
};

class QosOtaCommand : public ICommand {
public:
  explicit QosOtaCommand(ApiClient& apiClient);
  const char* getName() const override {
    return "qos-ota";
  }
  const char* getDescription() const override {
    return "Tests latency and loss to the Firmware Update API (Runs in background).";
  }
  bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ApiClient& m_apiClient;
};

#endif