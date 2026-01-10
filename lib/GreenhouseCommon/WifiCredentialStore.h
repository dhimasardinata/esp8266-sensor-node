#ifndef WIFI_CREDENTIAL_STORE_H
#define WIFI_CREDENTIAL_STORE_H

#include <Arduino.h>
#include <array>
#include <span>
#include <string_view>

// Maximum number of user-saved WiFi networks
constexpr size_t MAX_SAVED_NETWORKS = 5;
constexpr size_t WIFI_SSID_MAX_LEN = 33;
constexpr size_t WIFI_PASS_MAX_LEN = 65;

/**
 * @brief Represents a single WiFi credential with priority metadata
 */
struct WifiCredential {
    char ssid[WIFI_SSID_MAX_LEN] = {0};
    char password[WIFI_PASS_MAX_LEN] = {0};
    int32_t lastRssi = -100;  // Last seen signal strength
    bool isAvailable = false; // Currently visible in scan
    bool isBuiltIn = false;   // true for GH Atas/GH Bawah (not deletable)
    
    [[nodiscard]] bool isEmpty() const noexcept { return ssid[0] == '\0'; }
};

/**
 * @brief Priority levels for WiFi connection attempts
 */
enum class WifiPriority : uint8_t {
    PRIMARY_GH = 0,    // Node's primary greenhouse (highest)
    SECONDARY_GH = 1,  // Node's secondary greenhouse
    USER_SAVED = 2,    // User-configured networks (sorted by RSSI)
    PORTAL = 255       // No networks available (lowest)
};

/**
 * @brief Manages multiple WiFi credentials with priority-based selection
 * 
 * Features:
 * - Built-in GH Atas/GH Bawah with NODE_ID based priority
 * - Up to MAX_SAVED_NETWORKS user-saved credentials
 * - RSSI-based sorting for optimal connection
 * - Persistent storage with scrambling
 */
class WifiCredentialStore {
public:
    WifiCredentialStore();
    
    // Lifecycle
    void init();
    
    // User credential management
    [[nodiscard]] bool addCredential(std::string_view ssid, std::string_view password);
    [[nodiscard]] bool removeCredential(std::string_view ssid);
    [[nodiscard]] bool hasCredential(std::string_view ssid) const;
    
    // Priority list building (call after scan)
    void updateFromScan(int networkCount);
    
    // Get next credential to try (returns nullptr if exhausted)
    const WifiCredential* getNextCredential();
    void resetConnectionAttempt();
    
    // Getters
    size_t getSavedCount() const;
    size_t getTotalAvailableCount() const;
    const WifiCredential* getPrimaryGH() const { return &m_primaryGH; }
    const WifiCredential* getSecondaryGH() const { return &m_secondaryGH; }
    
    // For portal display
    std::span<WifiCredential> getSavedCredentials() { return m_savedCredentials; }

private:
    void loadFromFile();
    void saveToFile();
    void setupBuiltInCredentials();
    void sortByRssi();
    
    // Built-in credentials (based on NODE_ID)
    WifiCredential m_primaryGH;
    WifiCredential m_secondaryGH;
    
    // User-saved credentials
    std::array<WifiCredential, MAX_SAVED_NETWORKS> m_savedCredentials;
    
    // Connection attempt tracking
    uint8_t m_currentAttemptIndex = 0;
    bool m_triedPrimary = false;
    bool m_triedSecondary = false;
    
    static constexpr uint32_t CREDENTIAL_MAGIC = 0xCAFE1234;
};

#endif // WIFI_CREDENTIAL_STORE_H
