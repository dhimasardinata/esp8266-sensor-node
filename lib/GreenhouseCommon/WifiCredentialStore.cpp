#include "WifiCredentialStore.h"

#include <ESP8266WiFi.h>
#include <LittleFS.h>

#include <algorithm>

#include "node_config.h"
#include "Paths.h"
#include "utils.h"
#include "Logger.h"

namespace {
    // Built-in greenhouse credentials (stored in flash)
    const char GH_ATAS_SSID[] PROGMEM = "Your_Primary_SSID";
    const char GH_BAWAH_SSID[] PROGMEM = "Your_Secondary_SSID";
    const char GH_PASSWORD[] PROGMEM = "Your_WiFi_Password";
    
    // File header for validation
    struct CredentialFileHeader {
        uint32_t magic;
        uint8_t count;
        uint8_t reserved[3];
    };
}

WifiCredentialStore::WifiCredentialStore() {
    setupBuiltInCredentials();
}

void WifiCredentialStore::setupBuiltInCredentials() {
    // Primary GH based on NODE_ID
    // Node 1-5: Primary = GH Atas, Secondary = GH Bawah
    // Node 6-10: Primary = GH Bawah, Secondary = GH Atas
    const char* primarySsid_P = (NODE_ID <= 5) ? GH_ATAS_SSID : GH_BAWAH_SSID;
    const char* secondarySsid_P = (NODE_ID <= 5) ? GH_BAWAH_SSID : GH_ATAS_SSID;
    
    // Setup primary (use strcpy_P for PROGMEM strings)
    strcpy_P(m_primaryGH.ssid, primarySsid_P);
    strcpy_P(m_primaryGH.password, GH_PASSWORD);
    m_primaryGH.isBuiltIn = true;
    
    // Setup secondary
    strcpy_P(m_secondaryGH.ssid, secondarySsid_P);
    strcpy_P(m_secondaryGH.password, GH_PASSWORD);
    m_secondaryGH.isBuiltIn = true;
    
    // Copy to RAM for logging (PROGMEM strings can't be passed to %s directly)
    char pri[16], sec[16];
    strcpy_P(pri, primarySsid_P);
    strcpy_P(sec, secondarySsid_P);
    LOG_INFO("WIFI-STORE", F("NODE_ID=%d -> Primary: '%s', Secondary: '%s'"), 
             NODE_ID, pri, sec);
}

void WifiCredentialStore::init() {
    loadFromFile();
}

void WifiCredentialStore::loadFromFile() {
    if (!LittleFS.exists(Paths::WIFI_LIST)) {
        LOG_INFO("WIFI-STORE", F("No saved credentials file."));
        return;
    }
    
    File f = LittleFS.open(Paths::WIFI_LIST, "r");
    if (!f) {
        LOG_ERROR("WIFI-STORE", F("Failed to open credentials file."));
        return;
    }
    
    CredentialFileHeader header;
    if (f.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
        f.close();
        return;
    }
    
    if (header.magic != CREDENTIAL_MAGIC) {
        LOG_ERROR("WIFI-STORE", F("Invalid credentials file (magic mismatch)."));
        f.close();
        return;
    }
    
    size_t toRead = std::min(static_cast<size_t>(header.count), MAX_SAVED_NETWORKS);
    
    for (size_t i = 0; i < toRead; i++) {
        WifiCredential cred;
        if (f.read(reinterpret_cast<uint8_t*>(&cred), sizeof(WifiCredential)) == sizeof(WifiCredential)) {
            // Descramble password
            Utils::scramble_data(std::span{cred.password});
            m_savedCredentials[i] = cred;
        }
    }
    
    f.close();
    LOG_INFO("WIFI-STORE", F("Loaded %zu saved credentials."), toRead);
}

void WifiCredentialStore::saveToFile() {
    File f = LittleFS.open(Paths::WIFI_LIST, "w");
    if (!f) {
        LOG_ERROR("WIFI-STORE", F("Failed to create credentials file."));
        return;
    }
    
    // Count non-empty credentials
    uint8_t count = 0;
    for (const auto& cred : m_savedCredentials) {
        if (!cred.isEmpty()) count++;
    }
    
    CredentialFileHeader header;
    header.magic = CREDENTIAL_MAGIC;
    header.count = count;
    header.reserved[0] = header.reserved[1] = header.reserved[2] = 0;
    
    f.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
    
    for (const auto& cred : m_savedCredentials) {
        if (!cred.isEmpty()) {
            WifiCredential toWrite = cred;
            // Scramble password for storage
            Utils::scramble_data(std::span{toWrite.password});
            f.write(reinterpret_cast<const uint8_t*>(&toWrite), sizeof(WifiCredential));
        }
    }
    
    f.close();
    LOG_INFO("WIFI-STORE", F("Saved %d credentials."), count);
}

bool WifiCredentialStore::addCredential(std::string_view ssid, std::string_view password, bool hidden) {
    // Check if already exists
    for (auto& cred : m_savedCredentials) {
        if (!cred.isEmpty() && ssid == cred.ssid) {
            // Update password and hidden status
            Utils::copy_string(cred.password, password);
            cred.isHidden = hidden;
            saveToFile();
            LOG_INFO("WIFI-STORE", F("Updated credential for '%s' (hidden=%d)"), cred.ssid, hidden);
            return true;
        }
    }
    
    // Find empty slot
    for (auto& cred : m_savedCredentials) {
        if (cred.isEmpty()) {
            Utils::copy_string(cred.ssid, ssid);
            Utils::copy_string(cred.password, password);
            cred.isBuiltIn = false;
            cred.isHidden = hidden;
            saveToFile();
            LOG_INFO("WIFI-STORE", F("Added new credential for '%s' (hidden=%d)"), cred.ssid, hidden);
            return true;
        }
    }
    
    LOG_WARN("WIFI-STORE", F("No empty slots for new credential."));
    return false;
}

bool WifiCredentialStore::removeCredential(std::string_view ssid) {
    for (auto& cred : m_savedCredentials) {
        if (!cred.isEmpty() && ssid == cred.ssid) {
            cred = WifiCredential{};  // Clear
            saveToFile();
            LOG_INFO("WIFI-STORE", F("Removed credential for '%.*s'"), 
                    static_cast<int>(ssid.size()), ssid.data());
            return true;
        }
    }
    return false;
}

bool WifiCredentialStore::hasCredential(std::string_view ssid) const {
    // Check built-in
    if (ssid == m_primaryGH.ssid || ssid == m_secondaryGH.ssid) {
        return true;
    }
    
    // Check saved
    for (const auto& cred : m_savedCredentials) {
        if (!cred.isEmpty() && ssid == cred.ssid) {
            return true;
        }
    }
    return false;
}

// Helper to reset availability status before scan update
void WifiCredentialStore::resetAvailability() {
    m_primaryGH.isAvailable = false;
    m_primaryGH.lastRssi = -100;
    m_secondaryGH.isAvailable = false;
    m_secondaryGH.lastRssi = -100;
    
    for (auto& cred : m_savedCredentials) {
        cred.isAvailable = cred.isHidden;  // Hidden nets implicitly "available"
        cred.lastRssi = cred.isHidden ? -95 : -100;
    }
}

void WifiCredentialStore::updateFromScan(int networkCount) {
    resetAvailability();
    
    // Update credentials from scan results
    for (int i = 0; i < networkCount; i++) {
        String scannedSsid = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);
        
        // Check hardcoded credentials
        if (scannedSsid == m_primaryGH.ssid) { 
            m_primaryGH.isAvailable = true; 
            m_primaryGH.lastRssi = rssi; 
        }
        if (scannedSsid == m_secondaryGH.ssid) { 
            m_secondaryGH.isAvailable = true; 
            m_secondaryGH.lastRssi = rssi; 
        }
        
        // Check saved credentials
        for (auto& cred : m_savedCredentials) {
            if (!cred.isEmpty() && scannedSsid == cred.ssid) {
                cred.isAvailable = true;
                cred.lastRssi = rssi;
            }
        }
    }
    
    sortByRssi();
    
    LOG_INFO("WIFI-STORE", F("Scan update: Primary '%s' %s (%d), Secondary '%s' %s (%d)"),
             m_primaryGH.ssid, m_primaryGH.isAvailable ? "OK" : "nm", m_primaryGH.lastRssi,
             m_secondaryGH.ssid, m_secondaryGH.isAvailable ? "OK" : "nm", m_secondaryGH.lastRssi);
}

void WifiCredentialStore::sortByRssi() {
    std::sort(std::begin(m_savedCredentials), std::end(m_savedCredentials),
        [](const WifiCredential& a, const WifiCredential& b) {
            if (a.isEmpty() != b.isEmpty()) return !a.isEmpty();
            if (a.isAvailable != b.isAvailable) return a.isAvailable;
            return a.lastRssi > b.lastRssi;
        });
}

void WifiCredentialStore::resetConnectionAttempt() {
    m_currentAttemptIndex = 0;
    m_triedPrimary = false;
    m_triedSecondary = false;
}

const WifiCredential* WifiCredentialStore::getNextCredential() {
    // Priority 1: Primary GH (if available and not tried)
    if (!m_triedPrimary && m_primaryGH.isAvailable) {
        m_triedPrimary = true;
        LOG_INFO("WIFI-STORE", F("Next: Primary GH '%s' (RSSI: %d)"), 
                m_primaryGH.ssid, m_primaryGH.lastRssi);
        return &m_primaryGH;
    }
    
    // Priority 2: Secondary GH (if available and not tried)
    if (!m_triedSecondary && m_secondaryGH.isAvailable) {
        m_triedSecondary = true;
        LOG_INFO("WIFI-STORE", F("Next: Secondary GH '%s' (RSSI: %d)"), 
                m_secondaryGH.ssid, m_secondaryGH.lastRssi);
        return &m_secondaryGH;
    }
    
    // Priority 3+: Saved credentials (sorted by RSSI)
    while (m_currentAttemptIndex < MAX_SAVED_NETWORKS) {
        const auto& cred = m_savedCredentials[m_currentAttemptIndex];
        m_currentAttemptIndex++;
        
        if (!cred.isEmpty() && cred.isAvailable) {
            LOG_INFO("WIFI-STORE", F("Next: Saved '%s' (RSSI: %d, Hidden: %d)"), 
                    cred.ssid, cred.lastRssi, cred.isHidden);
            return &cred;
        }
    }
    
    // No more credentials to try
    LOG_WARN("WIFI-STORE", F("No more credentials available."));
    return nullptr;
}

size_t WifiCredentialStore::getSavedCount() const {
    size_t count = 0;
    for (const auto& cred : m_savedCredentials) {
        if (!cred.isEmpty()) count++;
    }
    return count;
}

size_t WifiCredentialStore::getTotalAvailableCount() const {
    size_t count = 0;
    if (m_primaryGH.isAvailable) count++;
    if (m_secondaryGH.isAvailable) count++;
    for (const auto& cred : m_savedCredentials) {
        if (!cred.isEmpty() && cred.isAvailable) count++;
    }
    return count;
}
