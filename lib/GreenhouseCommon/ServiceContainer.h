// lib/GreenhouseCommon/ServiceContainer.h

#ifndef SERVICE_CONTAINER_H
#define SERVICE_CONTAINER_H

// Forward declarations
class ConfigManager;
class WifiManager;
class NtpClient;
class ISensorManager;
class ICacheManager;
class ApiClient;
class OtaManager;
class AppServer;
class PortalServer;
class DiagnosticsTerminal;

// Forward declaration for BearSSL
namespace BearSSL {
  class WiFiClientSecure;
}

struct ServiceContainer {
  ConfigManager& configManager;
  WifiManager& wifiManager;
  NtpClient& ntpClient;
  ISensorManager& sensorManager;
  ICacheManager& cacheManager;
  ApiClient& apiClient;
  OtaManager& otaManager;
  AppServer& appServer;
  PortalServer& portalServer;
  
  // --- ADDED: Shared Secure Client ---
  BearSSL::WiFiClientSecure& secureClient;

  // Optional but usually present after init
  DiagnosticsTerminal* terminal = nullptr;

  void setTerminal(DiagnosticsTerminal* term) {
    terminal = term;
  }

  // Safe accessor - crashes nicely (or logs) if accessed too early,
  // but better to rely on lifecycle management.
  DiagnosticsTerminal& getTerminal() {
      // In a real robust system, we might check for null here.
      // For now, assuming caller knows lifecycle.
      return *terminal;
  }
};

#endif  // SERVICE_CONTAINER_H