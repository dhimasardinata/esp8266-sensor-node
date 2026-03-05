#include "REDACTED"

#include <ESP8266WiFi.h>
#include <LittleFS.h>

#include <algorithm>
#include <cstring>
#include <new>

#include "Logger.h"
#include "Paths.h"
#include "node_config.h"
#include "utils.h"

namespace {
  // Built-in credentials for primary infrastructure points (overridable via build flags).
#ifndef BUILTIN_GH_ATAS_SSID
#define BUILTIN_GH_ATAS_SSID "REDACTED"
#endif
#ifndef BUILTIN_GH_BAWAH_SSID
#define BUILTIN_GH_BAWAH_SSID "REDACTED"
#endif
#ifndef BUILTIN_GH_PASSWORD
#define BUILTIN_GH_PASSWORD "REDACTED"
#endif
#ifndef ENABLE_BUILTIN_WIFI_CREDENTIALS
#define ENABLE_BUILTIN_WIFI_CREDENTIALS 1
#endif

  const char GH_ATAS_SSID[] PROGMEM = REDACTED
  const char GH_BAWAH_SSID[] PROGMEM = REDACTED
  const char GH_PASSWORD[] PROGMEM = REDACTED

  // File header for validation
  struct CredentialFileHeader {
    uint32_t magic;
    uint8_t count;
    uint8_t reserved[3];
  };

  uint32_t fnv1a32(const char* s, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
      h ^= static_cast<uint8_t>(s[i]);
      h *= 16777619u;
    }
    return h;
  }
}  // namespace

WifiCredentialStore:REDACTED
  setupBuiltInCredentials();
}

void WifiCredentialStore:REDACTED
#if !ENABLE_BUILTIN_WIFI_CREDENTIALS
  m_primaryGH.ssid[0] = REDACTED
  m_primaryGH.password[0] = REDACTED
  m_primaryGH.setBuiltIn(false);
  m_secondaryGH.ssid[0] = REDACTED
  m_secondaryGH.password[0] = REDACTED
  m_secondaryGH.setBuiltIn(false);
  LOG_WARN("REDACTED", F("REDACTED"));
  return;
#endif
  // Primary GH based on NODE_ID
  // Assign primary/secondary credentials based on NODE_ID to ensure load balancing.
  // Nodes 1-5 prefer GH Atas; Nodes 6-10 prefer GH Bawah.
  const char* primarySsid_P = REDACTED
  const char* secondarySsid_P = REDACTED

  // Initialize primary credentials.
  strcpy_P(m_primaryGH.ssid, primarySsid_P);
  strcpy_P(m_primaryGH.password, GH_PASSWORD);
  m_primaryGH.setBuiltIn(true);

  // Setup secondary
  strcpy_P(m_secondaryGH.ssid, secondarySsid_P);
  strcpy_P(m_secondaryGH.password, GH_PASSWORD);
  m_secondaryGH.setBuiltIn(true);

  // Log configuration (buffer sized dynamically)
  char pri[33] = {0};
  char sec[33] = {0};
  strncpy_P(pri, primarySsid_P, sizeof(pri) - 1);
  strncpy_P(sec, secondarySsid_P, sizeof(sec) - 1);
  LOG_INFO("WIFI-STORE", F("NODE_ID=REDACTED
}

void WifiCredentialStore:REDACTED
  // Lazy load saved credentials on first access to free heap at boot.
}

bool WifiCredentialStore:REDACTED
  if (m_savedLoaded) {
    return true;
  }
  auto* self = const_cast<WifiCredentialStore*>(this);
  std::unique_ptr<WifiCredential[]> creds(new (std::nothrow) WifiCredential[MAX_SAVED_NETWORKS]());
  if (!creds) {
    LOG_WARN("REDACTED", F("REDACTED"));
    return false;
  }
  self->m_savedCredentials.swap(creds);
  self->m_savedLoaded = true;
  self->loadFromFile();
  return true;
}

std::span<WifiCredential> WifiCredentialStore::savedSpan() {
  if (!ensureSavedLoaded()) {
    return {};
  }
  return {m_savedCredentials.get(), MAX_SAVED_NETWORKS};
}

std::span<const WifiCredential> WifiCredentialStore::savedSpan() const {
  if (!ensureSavedLoaded()) {
    return {};
  }
  return {m_savedCredentials.get(), MAX_SAVED_NETWORKS};
}

std::span<WifiCredential> WifiCredentialStore::getSavedCredentials() {
  return savedSpan();
}

std::span<const WifiCredential> WifiCredentialStore::getSavedCredentials() const {
  return savedSpan();
}

void WifiCredentialStore:REDACTED
  if (!m_savedCredentials) {
    return;
  }
  m_savedCredentials.reset();
  m_savedLoaded = false;
}

void WifiCredentialStore:REDACTED
  if (!m_savedCredentials) {
    return;
  }
  if (!LittleFS.exists(Paths::WIFI_LIST)) {
    LOG_INFO("REDACTED", F("REDACTED"));
    return;
  }

  File f = LittleFS.open(Paths::WIFI_LIST, "r");
  if (!f) {
    LOG_ERROR("REDACTED", F("REDACTED"));
    return;
  }

  CredentialFileHeader header;
  if (f.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
    f.close();
    return;
  }

  if (header.magic != CREDENTIAL_MAGIC) {
    LOG_ERROR("REDACTED", F("REDACTED"));
    f.close();
    return;
  }

  size_t toRead = std::min(static_cast<size_t>(header.count), MAX_SAVED_NETWORKS);

  for (size_t i = 0; i < toRead; i++) {
    WifiCredential cred;
    if (f.read(reinterpret_cast<uint8_t*>(&cred), sizeof(WifiCredential)) =REDACTED
      // Descramble password
      Utils::scramble_data(std::span{cred.password});
      m_savedCredentials[i] = cred;
    }
  }

  f.close();
  LOG_INFO("REDACTED", F("REDACTED"), toRead);
}

// FIX: Implemented Atomic Write Pattern (Tmp -> Rename)
void WifiCredentialStore:REDACTED
  auto saved = savedSpan();
  if (saved.empty()) {
    return;
  }
  // 1. Write to temporary file
  const char* tmpPath = "/wifi_list.tmp";
  File f = LittleFS.open(tmpPath, "w");
  if (!f) {
    LOG_ERROR("REDACTED", F("REDACTED"));
    return;
  }

  // Count non-empty credentials
  uint8_t count = 0;
  for (const auto& cred : saved) {
    if (!cred.isEmpty())
      count++;
  }

  CredentialFileHeader header;
  header.magic = CREDENTIAL_MAGIC;
  header.count = count;
  header.reserved[0] = header.reserved[1] = header.reserved[2] = 0;

  // VERIFY: Header Write
  if (f.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
    LOG_ERROR("REDACTED", F("REDACTED"));
    f.close();
    LittleFS.remove(tmpPath);  // Transaction Abort
    return;
  }

  for (const auto& cred : saved) {
    if (!cred.isEmpty()) {
      WifiCredential toWrite = REDACTED
      // Encrypt password before storage.
      Utils::scramble_data(std::span{toWrite.password});

      // VERIFY: Data Write
      if (f.write(reinterpret_cast<const uint8_t*>(&toWrite), sizeof(WifiCredential)) != REDACTED
        LOG_ERROR("REDACTED", F("REDACTED"));
        f.close();
        LittleFS.remove(tmpPath);  // Transaction Abort
        return;
      }
    }
  }

  // VERIFY: Flush and check physical size
  f.flush();
  size_t finalSize = f.size();
  f.close();

  size_t expectedSize = sizeof(CredentialFileHeader) + (count * sizeof(WifiCredential));
  if (finalSize != expectedSize) {
    LOG_ERROR("REDACTED", F("REDACTED"), expectedSize, finalSize);
    LittleFS.remove(tmpPath);  // Transaction Abort
    return;
  }

  // 2. Atomic Rename
  // NOTE: LittleFS (v2+) rename is atomic (replace). Explicit remove() is dangerous
  // as it creates a window where data is lost if power fails.
  if (LittleFS.rename(tmpPath, Paths::WIFI_LIST)) {
    LOG_INFO("REDACTED", F("REDACTED"), count);
  } else {
    LOG_ERROR("REDACTED", F("REDACTED"));
    LittleFS.remove(tmpPath);  // Clean up temp if rename fails
  }
}

bool WifiCredentialStore:REDACTED
  auto saved = savedSpan();
  if (saved.empty()) {
    return false;
  }
  // Check if already exists
  for (auto& cred : saved) {
    if (!cred.isEmpty() && ssid =REDACTED
      // Update password and hidden status
      Utils::copy_string(cred.password, password);
      cred.setHidden(hidden);
      saveToFile();
      LOG_INFO("WIFI-STORE", F("Updated credential for '%s' (hidden=REDACTED
      return true;
    }
  }

  // Find empty slot
  for (auto& cred : saved) {
    if (cred.isEmpty()) {
      Utils::copy_string(cred.ssid, ssid);
      Utils::copy_string(cred.password, password);
      cred.setBuiltIn(false);
      cred.setHidden(hidden);
      saveToFile();
      LOG_INFO("WIFI-STORE", F("Added new credential for '%s' (hidden=REDACTED
      return true;
    }
  }

  LOG_WARN("REDACTED", F("REDACTED"));
  return false;
}

bool WifiCredentialStore:REDACTED
  auto saved = savedSpan();
  for (auto& cred : saved) {
    if (!cred.isEmpty() && ssid =REDACTED
      cred = WifiCredential{};  // Clear
      saveToFile();
      LOG_INFO("REDACTED", F("REDACTED"), static_cast<int>(ssid.size()), ssid.data());
      return true;
    }
  }
  return false;
}

bool WifiCredentialStore:REDACTED
  // Check built-in
  if (ssid =REDACTED
    return true;
  }

  // Check saved
  auto saved = savedSpan();
  for (const auto& cred : saved) {
    if (!cred.isEmpty() && ssid =REDACTED
      return true;
    }
  }
  return false;
}

// Helper to reset availability status before scan update
void WifiCredentialStore:REDACTED
  m_primaryGH.setAvailable(false);
  m_primaryGH.lastRssi = -100;
  m_secondaryGH.setAvailable(false);
  m_secondaryGH.lastRssi = -100;

  auto saved = savedSpan();
  for (auto& cred : saved) {
    cred.setAvailable(cred.isHidden());  // Hidden networks are assumed available until proven otherwise.
    cred.lastRssi = cred.isHidden() ? -95 : -100;
  }
}

void WifiCredentialStore:REDACTED
  resetAvailability();

  const uint8_t primaryLen = static_cast<uint8_t>(strnlen(m_primaryGH.ssid, WIFI_SSID_MAX_LEN - 1));
  const uint8_t secondaryLen = static_cast<uint8_t>(strnlen(m_secondaryGH.ssid, WIFI_SSID_MAX_LEN - 1));
  const uint32_t primaryHash = (primaryLen > 0) ? fnv1a32(m_primaryGH.ssid, primaryLen) : 0;
  const uint32_t secondaryHash = (secondaryLen > 0) ? fnv1a32(m_secondaryGH.ssid, secondaryLen) : 0;

  struct SavedSlot {
    WifiCredential* cred = REDACTED
    uint8_t len = 0;
    uint32_t hash = 0;
  };
  constexpr size_t kTableSize = (MAX_SAVED_NETWORKS <= 8) ? 16 : 32;
  static_assert((kTableSize & (kTableSize - 1)) == 0, "kTableSize must be power-of-two");
  static_assert(kTableSize > MAX_SAVED_NETWORKS, "kTableSize must exceed MAX_SAVED_NETWORKS");
  SavedSlot table[kTableSize] = {};
  auto insert = [&](WifiCredential* cred, uint8_t len, uint32_t hash) {
    size_t idx = hash & (kTableSize - 1);
    for (size_t probe = 0; probe < kTableSize; ++probe) {
      if (!table[idx].cred) {
        table[idx] = {cred, len, hash};
        return;
      }
      idx = (idx + 1) & (kTableSize - 1);
    }
  };
  auto saved = savedSpan();
  for (auto& cred : saved) {
    if (!cred.isEmpty()) {
      size_t len = strnlen(cred.ssid, WIFI_SSID_MAX_LEN - 1);
      insert(&cred, static_cast<uint8_t>(len), fnv1a32(cred.ssid, len));
    }
  }

  // Update credentials from scan results
  String ssidTmp;
  ssidTmp.reserve(WIFI_SSID_MAX_LEN);
  for (int i = 0; i < networkCount; i++) {
    ssidTmp = REDACTED
    if (ssidTmp.length() =REDACTED
      continue;
    const char* scannedSsid = REDACTED
    if (!scannedSsid || scannedSsid[0] =REDACTED
      continue;
    int32_t rssi = WiFi.RSSI(i);
    size_t rawLen = ssidTmp.length();
    if (rawLen >= WIFI_SSID_MAX_LEN) {
      rawLen = WIFI_SSID_MAX_LEN - 1;
    }
    const uint8_t ssidLen = REDACTED
    const uint32_t ssidHash = REDACTED

    // Check hardcoded credentials
    if (ssidLen =REDACTED
      m_primaryGH.setAvailable(true);
      m_primaryGH.lastRssi = rssi;
    }
    if (ssidLen =REDACTED
      m_secondaryGH.setAvailable(true);
      m_secondaryGH.lastRssi = rssi;
    }

    // Check saved credentials (hashed lookup)
    size_t idx = ssidHash & (kTableSize - 1);
    for (size_t probe = 0; probe < kTableSize; ++probe) {
      SavedSlot& slot = table[idx];
      if (!slot.cred) {
        break;
      }
      if (slot.hash == ssidHash && slot.len == ssidLen && memcmp(scannedSsid, slot.cred->ssid, ssidLen) == 0) {
        slot.cred->setAvailable(true);
        slot.cred->lastRssi = rssi;
        break;
      }
      idx = (idx + 1) & (kTableSize - 1);
    }
  }

  sortByRssi();

  LOG_INFO("REDACTED",
           F("Scan update: Primary '%s' %s (%d), Secondary '%s' %s (%d)"),
           m_primaryGH.ssid,
           m_primaryGH.isAvailable() ? "OK" : "nm",
           m_primaryGH.lastRssi,
           m_secondaryGH.ssid,
           m_secondaryGH.isAvailable() ? "OK" : "nm",
           m_secondaryGH.lastRssi);
}

void WifiCredentialStore:REDACTED
  resetAvailability();

  const uint8_t primaryLen = static_cast<uint8_t>(strnlen(m_primaryGH.ssid, WIFI_SSID_MAX_LEN - 1));
  const uint8_t secondaryLen = static_cast<uint8_t>(strnlen(m_secondaryGH.ssid, WIFI_SSID_MAX_LEN - 1));
  const uint32_t primaryHash = (primaryLen > 0) ? fnv1a32(m_primaryGH.ssid, primaryLen) : 0;
  const uint32_t secondaryHash = (secondaryLen > 0) ? fnv1a32(m_secondaryGH.ssid, secondaryLen) : 0;

  struct SavedSlot {
    WifiCredential* cred = REDACTED
    uint8_t len = 0;
    uint32_t hash = 0;
  };
  constexpr size_t kTableSize = (MAX_SAVED_NETWORKS <= 8) ? 16 : 32;
  static_assert((kTableSize & (kTableSize - 1)) == 0, "kTableSize must be power-of-two");
  static_assert(kTableSize > MAX_SAVED_NETWORKS, "kTableSize must exceed MAX_SAVED_NETWORKS");
  SavedSlot table[kTableSize] = {};
  auto insert = [&](WifiCredential* cred, uint8_t len, uint32_t hash) {
    size_t idx = hash & (kTableSize - 1);
    for (size_t probe = 0; probe < kTableSize; ++probe) {
      if (!table[idx].cred) {
        table[idx] = {cred, len, hash};
        return;
      }
      idx = (idx + 1) & (kTableSize - 1);
    }
  };
  auto saved = savedSpan();
  for (auto& cred : saved) {
    if (!cred.isEmpty()) {
      size_t len = strnlen(cred.ssid, WIFI_SSID_MAX_LEN - 1);
      insert(&cred, static_cast<uint8_t>(len), fnv1a32(cred.ssid, len));
    }
  }

  if (list && count > 0) {
    for (size_t i = 0; i < count; ++i) {
      const char* scannedSsid = REDACTED
      if (!scannedSsid || scannedSsid[0] =REDACTED
        continue;
      size_t rawLen = list[i].len;
      if (rawLen == 0) {
        rawLen = strnlen(scannedSsid, WIFI_SSID_MAX_LEN - 1);
      }
      if (rawLen == 0)
        continue;
      if (rawLen >= WIFI_SSID_MAX_LEN) {
        rawLen = WIFI_SSID_MAX_LEN - 1;
      }
      const uint8_t ssidLen = REDACTED
      const uint32_t ssidHash = REDACTED
      const int32_t rssi = list[i].rssi;

      if (ssidLen =REDACTED
          memcmp(scannedSsid, m_primaryGH.ssid, ssidLen) =REDACTED
        m_primaryGH.setAvailable(true);
        m_primaryGH.lastRssi = rssi;
      }
      if (ssidLen =REDACTED
          memcmp(scannedSsid, m_secondaryGH.ssid, ssidLen) =REDACTED
        m_secondaryGH.setAvailable(true);
        m_secondaryGH.lastRssi = rssi;
      }

      size_t idx = ssidHash & (kTableSize - 1);
      for (size_t probe = 0; probe < kTableSize; ++probe) {
        SavedSlot& slot = table[idx];
        if (!slot.cred) {
          break;
        }
        if (slot.hash == ssidHash && slot.len == ssidLen &&
            memcmp(scannedSsid, slot.cred->ssid, ssidLen) =REDACTED
          slot.cred->setAvailable(true);
          slot.cred->lastRssi = rssi;
          break;
        }
        idx = (idx + 1) & (kTableSize - 1);
      }
    }
  }

  sortByRssi();

  LOG_INFO("REDACTED",
           F("Lite scan: Primary '%s' %s (%d), Secondary '%s' %s (%d)"),
           m_primaryGH.ssid,
           m_primaryGH.isAvailable() ? "OK" : "nm",
           m_primaryGH.lastRssi,
           m_secondaryGH.ssid,
           m_secondaryGH.isAvailable() ? "OK" : "nm",
           m_secondaryGH.lastRssi);
}

void WifiCredentialStore:REDACTED
  auto saved = savedSpan();
  if (saved.empty()) {
    return;
  }
  auto better = [](const WifiCredential& a, const WifiCredential& b) {
    if (a.isEmpty() != b.isEmpty())
      return !a.isEmpty();
    if (a.isAvailable() != b.isAvailable())
      return a.isAvailable();
    return a.lastRssi > b.lastRssi;
  };

  for (size_t i = 1; i < saved.size(); ++i) {
    WifiCredential key = REDACTED
    size_t j = i;
    while (j > 0 && better(key, saved[j - 1])) {
      saved[j] = saved[j - 1];
      --j;
    }
    saved[j] = key;
  }
}

void WifiCredentialStore:REDACTED
  m_currentAttemptIndex = 0;
  m_triedPrimary = false;
  m_triedSecondary = false;
}

const WifiCredential* WifiCredentialStore:REDACTED
  // Priority 1: Primary Greenhouse (if available and untried).
  if (!m_triedPrimary && m_primaryGH.isAvailable()) {
    m_triedPrimary = true;
    LOG_INFO("WIFI-STORE", F("Next: REDACTED
    return &m_primaryGH;
  }

  // Priority 2: Secondary Greenhouse (if available and untried).
  if (!m_triedSecondary && m_secondaryGH.isAvailable()) {
    m_triedSecondary = true;
    LOG_INFO("WIFI-STORE", F("Next: REDACTED
    return &m_secondaryGH;
  }

  // Priority 3: Saved User Credentials (RSSI sorted).
  auto saved = savedSpan();
  while (m_currentAttemptIndex < saved.size()) {
    const auto& cred = saved[m_currentAttemptIndex];
    m_currentAttemptIndex++;

    if (!cred.isEmpty() && cred.isAvailable()) {
      LOG_INFO("WIFI-STORE", F("Next: REDACTED
      return &cred;
    }
  }

  // No more credentials to try
  LOG_WARN("REDACTED", F("REDACTED"));
  return nullptr;
}

size_t WifiCredentialStore:REDACTED
  size_t count = 0;
  auto saved = savedSpan();
  for (const auto& cred : saved) {
    if (!cred.isEmpty())
      count++;
  }
  return count;
}

size_t WifiCredentialStore:REDACTED
  size_t count = 0;
  if (m_primaryGH.isAvailable())
    count++;
  if (m_secondaryGH.isAvailable())
    count++;
  auto saved = savedSpan();
  for (const auto& cred : saved) {
    if (!cred.isEmpty() && cred.isAvailable())
      count++;
  }
  return count;
}
