#ifndef QOS_COMMAND_H
#define QOS_COMMAND_H

#include "ICommand.h"

class ApiClient;

class QosUploadCommand : public ICommand {
public:
  explicit QosUploadCommand(ApiClient& apiClient);
    PGM_P getName_P() const override { return PSTR("qosupload"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("qosupload"); }
    PGM_P getDescription_P() const override {
    return PSTR("Tests latency and loss to the Data Upload API (Runs in background).");
  }
  CommandSection helpSection() const override { return CommandSection::SYSTEM; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ApiClient& m_apiClient;
};

class QosOtaCommand : REDACTED
public:
  explicit QosOtaCommand(ApiClient& apiClient);
    PGM_P getName_P() const override { return PSTR("REDACTED"); }
    uint32_t getNameHash() const override { return CompileTimeUtils::ct_hash("qosota"); }
    PGM_P getDescription_P() const override {
    return PSTR("Tests latency and loss to the Firmware Update API (Runs in background).");
  }
  CommandSection helpSection() const override { return CommandSection::SYSTEM; }
    bool requiresAuth() const override {
    return true;
  }
  void execute(const CommandContext& context) override;

private:
  ApiClient& m_apiClient;
};

#endif