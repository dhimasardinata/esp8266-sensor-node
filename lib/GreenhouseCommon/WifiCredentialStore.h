#ifndef WIFI_CREDENTIAL_STORE_H
#define WIFI_CREDENTIAL_STORE_H

#include <Arduino.h>
#include <array>
#include <memory>
#include <span>
#include <string_view>

// Maximum number of user-saved WiFi networks
constexpr size_t MAX_SAVED_NETWORKS = 5;
constexpr size_t WIFI_SSID_MAX_LEN = REDACTED
constexpr size_t WIFI_PASS_MAX_LEN = REDACTED

// Represents a single WiFi credential with connection metadata.
struct WifiCredential {
    char ssid[WIFI_SSID_MAX_LEN] = REDACTED
    char password[WIFI_PASS_MAX_LEN] = REDACTED
    int16_t lastRssi = -100;  // Last seen signal strength (fits in int16_t)
    uint8_t flags = 0;        // bit flags (available/built-in/hidden)

    static constexpr uint8_t FLAG_AVAILABLE = 1u << 0;
    static constexpr uint8_t FLAG_BUILTIN = 1u << 1;
    static constexpr uint8_t FLAG_HIDDEN = 1u << 2;
    
    [[nodiscard]] bool isAvailable() const noexcept { return (flags & FLAG_AVAILABLE) != 0; }
    [[nodiscard]] bool isBuiltIn() const noexcept { return (flags & FLAG_BUILTIN) != 0; }
    [[nodiscard]] bool isHidden() const noexcept { return (flags & FLAG_HIDDEN) != 0; }

    void setAvailable(bool v) noexcept {
      flags = static_cast<uint8_t>(v ? (flags | FLAG_AVAILABLE) : (flags & ~FLAG_AVAILABLE));
    }
    void setBuiltIn(bool v) noexcept {
      flags = static_cast<uint8_t>(v ? (flags | FLAG_BUILTIN) : (flags & ~FLAG_BUILTIN));
    }
    void setHidden(bool v) noexcept {
      flags = static_cast<uint8_t>(v ? (flags | FLAG_HIDDEN) : (flags & ~FLAG_HIDDEN));
    }
    
    [[nodiscard]] bool isEmpty() const noexcept { return ssid[0] =REDACTED
};

namespace {
  constexpr size_t WIFI_CRED_PRE_INT = REDACTED
  constexpr size_t WIFI_CRED_INT_ALIGN = REDACTED
  constexpr size_t WIFI_CRED_PAD_BEFORE_INT =
      (WIFI_CRED_INT_ALIGN - (WIFI_CRED_PRE_INT % WIFI_CRED_INT_ALIGN)) % WIFI_CRED_INT_ALIGN;
  constexpr size_t WIFI_CRED_RAW_SIZE =
      WIFI_CRED_PRE_INT + WIFI_CRED_PAD_BEFORE_INT + sizeof(int16_t) + sizeof(uint8_t);
  constexpr size_t WIFI_CRED_STRUCT_ALIGN = REDACTED
  constexpr size_t WIFI_CRED_EXPECTED_SIZE =
      (WIFI_CRED_RAW_SIZE + (WIFI_CRED_STRUCT_ALIGN - 1)) & ~(WIFI_CRED_STRUCT_ALIGN - 1);
}
static_assert(sizeof(WifiCredential) =REDACTED
              "REDACTED");

/**
 * @brief Priority levels for WiFi connection attempts
 */
enum class WifiPriority : REDACTED
    PRIMARY_GH = 0,    // High priority: Assigned greenhouse network.
    SECONDARY_GH = 1,  // Medium priority: Secondary greenhouse network.
    USER_SAVED = 2,    // Normal priority: User-configured networks.
    PORTAL = 255       // Fallback: No connection available.
};

// Manages WiFi credentials with priority-based selection and persistence.
// Handles built-in greenhouse credentials and user-defined networks.
class WifiCredentialStore {
public:
    WifiCredentialStore();
    
    // Lifecycle
    void init();
    
    // User credential management
    [[nodiscard]] bool addCredential(std::string_view ssid, std::string_view password, bool hidden = false);
    [[nodiscard]] bool removeCredential(std::string_view ssid);
    [[nodiscard]] bool hasCredential(std::string_view ssid) const;
    
    // Update availability based on scan results.
    void updateFromScan(int networkCount);
    struct ScanEntry {
      const char* ssid = REDACTED
      uint8_t len = 0;
      int32_t rssi = -100;
    };
    void updateFromScanList(const ScanEntry* list, size_t count);
    
    // Retrieve the next best credential to attempt.
    const WifiCredential* getNextCredential();
    void resetConnectionAttempt();
    
    // Getters
    size_t getSavedCount() const;
    size_t getTotalAvailableCount() const;
    const WifiCredential* getPrimaryGH() const { return &m_primaryGH; }
    const WifiCredential* getSecondaryGH() const { return &m_secondaryGH; }
    
    // For portal display
    std::span<WifiCredential> getSavedCredentials();
    std::span<const WifiCredential> getSavedCredentials() const;
    void releaseSavedCredentials();

private:
    void loadFromFile();
    void saveToFile();
    void setupBuiltInCredentials();
    void sortByRssi();
    void resetAvailability();
    bool ensureSavedLoaded() const;
    std::span<WifiCredential> savedSpan();
    std::span<const WifiCredential> savedSpan() const;
    
    // Built-in credentials (based on NODE_ID)
    WifiCredential m_primaryGH;
    WifiCredential m_secondaryGH;
    
    // User-saved credentials
    mutable std::unique_ptr<WifiCredential[]> m_savedCredentials;
    mutable bool m_savedLoaded = false;
    
    // Connection attempt tracking
    uint8_t m_currentAttemptIndex = 0;
    bool m_triedPrimary = false;
    bool m_triedSecondary = false;
    
    static constexpr uint32_t CREDENTIAL_MAGIC = 0xCAFE1236; // Increment for layout change
};

#endif // WIFI_CREDENTIAL_STORE_H
