#ifndef IAUTH_MANAGER_H
#define IAUTH_MANAGER_H

#include <cstdint>

// Interface for authentication management
// MEMORY OPTIMIZATION: Allows commands to use consolidated client state
// without needing direct access to maps or vectors
class IAuthManager {
public:
  virtual ~IAuthManager() = default;
  
  // Check if client is authenticated
  virtual bool isClientAuthenticated(uint32_t clientId) const = 0;
  
  // Set client authentication status
  virtual void setClientAuthenticated(uint32_t clientId, bool authenticated) = 0;
  
  // Check if client is locked out from login attempts
  virtual bool isClientLockedOut(uint32_t clientId) const = 0;
  
  // Record a failed login attempt
  virtual void recordFailedLogin(uint32_t clientId) = 0;
  
  // Clear failed login attempts (called on successful login)
  virtual void clearFailedLogins(uint32_t clientId) = 0;
};

#endif  // IAUTH_MANAGER_H
