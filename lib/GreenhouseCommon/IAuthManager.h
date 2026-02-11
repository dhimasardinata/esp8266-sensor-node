#ifndef IAUTH_MANAGER_H
#define IAUTH_MANAGER_H

#include <cstdint>

// ============================================================================
// CRTP Base Class for Zero-Overhead Auth Interface
// ============================================================================
// CRTP provides compile-time polymorphism without virtual function overhead:
// - No vtable pointer (saves 4 bytes per object)
// - No indirect function calls (enables inlining)

template <typename Derived>
class IAuthManager {
public:
  bool isClientAuthenticated(uint32_t clientId) const {
    return static_cast<const Derived*>(this)->isClientAuthenticatedImpl(clientId);
  }
  
  void setClientAuthenticated(uint32_t clientId, bool authenticated) {
    static_cast<Derived*>(this)->setClientAuthenticatedImpl(clientId, authenticated);
  }
  
  bool isClientLockedOut(uint32_t clientId) const {
    return static_cast<const Derived*>(this)->isClientLockedOutImpl(clientId);
  }
  
  void recordFailedLogin(uint32_t clientId) {
    static_cast<Derived*>(this)->recordFailedLoginImpl(clientId);
  }
  
  void clearFailedLogins(uint32_t clientId) {
    static_cast<Derived*>(this)->clearFailedLoginsImpl(clientId);
  }

protected:
  IAuthManager() = REDACTED
  ~IAuthManager() = REDACTED
  IAuthManager(const IAuthManager&) = REDACTED
  IAuthManager& operator=REDACTED
};

#endif  // IAUTH_MANAGER_H
