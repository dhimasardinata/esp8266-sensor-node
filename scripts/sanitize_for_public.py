#!/usr/bin/env python3
"""
Sanitize Repository Script
Creates a COPY of the repository with all sensitive credentials replaced by placeholders.
Original repository remains unchanged.
"""

import os
import shutil
import re
from pathlib import Path

# Configuration
SOURCE_DIR = Path(r"c:\Users\ASUS\Documents\Dhimas\TA\node-medini")
TARGET_DIR = Path(r"c:\Users\ASUS\Documents\Dhimas\TA\esp8266-sensor-node")  # Generic name

# Files to completely replace with templates
TEMPLATE_FILES = {
    "node_id.ini": """[settings]
gh_id = 1
node_id = 1
ota_ip = 192.168.1.100
""",
    "include/calibration.h": '''/**
 * @file calibration.h
 * @brief Sensor calibration defaults (per-node)
 */
#ifndef COMPILED_CALIBRATION_DEFAULTS_H
#define COMPILED_CALIBRATION_DEFAULTS_H

#include "node_config.h"
#ifndef NODE_ID
#error "NODE_ID is not defined! Please define it in platformio.ini"
#endif

namespace CompiledDefaults {
  // Default calibration values - customize per node
  constexpr float TEMP_OFFSET = 0.0f;
  constexpr float HUMIDITY_OFFSET = 0.0f;
  constexpr float LUX_SCALING_FACTOR = 1.0f;
}  // namespace CompiledDefaults
#endif  // COMPILED_CALIBRATION_DEFAULTS_H
''',
    "include/certs.h": """#ifndef CERTS_H
#define CERTS_H

#include <Arduino.h>

// PLACEHOLDER: Generate your own SSL certificate and key
// Use: openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 3650 -nodes
// Then run: python scripts/convert_certs.py

const uint8_t server_cert[] PROGMEM = {
    // YOUR CERTIFICATE DATA HERE
    0x00
};
const size_t server_cert_len = sizeof(server_cert);

const uint8_t server_key[] PROGMEM = {
    // YOUR PRIVATE KEY DATA HERE  
    0x00
};
const size_t server_key_len = sizeof(server_key);

#endif  // CERTS_H
""",
    "lib/GreenhouseCommon/CryptoUtils.h": '''#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <Arduino.h>
#include <bearssl/bearssl.h>

#include <optional>
#include <string_view>
#include <vector>

namespace CryptoUtils {

  // PLACEHOLDER: Replace with your own 32-byte AES-256 key
  // Must match the key in client-side JavaScript (data/crypto.js)
  // Generate with: python -c "import os; print([hex(b) for b in os.urandom(32)])"
  constexpr uint8_t AES_KEY[] = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
  };

  struct EncryptedPayload {
    std::vector<uint8_t> iv;
    std::vector<uint8_t> ciphertext;
  };

  class AES_CBC_Cipher {
  public:
    explicit AES_CBC_Cipher(std::string_view key);
    ~AES_CBC_Cipher();

    AES_CBC_Cipher(const AES_CBC_Cipher&) = delete;
    AES_CBC_Cipher& operator=(const AES_CBC_Cipher&) = delete;

    [[nodiscard]] std::optional<EncryptedPayload> encrypt(std::string_view plaintext) const;
    [[nodiscard]] std::optional<std::vector<uint8_t>> decrypt(const EncryptedPayload& payload) const;

  private:
    mutable br_aes_ct_cbcenc_keys m_enc_ctx;
    mutable br_aes_ct_cbcdec_keys m_dec_ctx;
  };

  String serialize_payload(const EncryptedPayload& payload);
  std::optional<EncryptedPayload> deserialize_payload(std::string_view serialized);

}  // namespace CryptoUtils

#endif  // CRYPTO_UTILS_H
''',
}

# Directories to exclude from copying
EXCLUDE_DIRS = {".git", ".pio", "build", "node_modules", "__pycache__", ".vscode"}

# Files to exclude
EXCLUDE_FILES = {".env", ".env.local", "secrets.h"}

# Patterns to sanitize in remaining files
SANITIZE_PATTERNS = [
    # Project-specific URLs
    (r'atomic\.web\.id', 'example.com'),
    (r'https://atomic\.web\.id/api/sensor', 'https://example.com/api/sensor'),
    (r'https://atomic\.web\.id/api/get-file/', 'https://example.com/api/firmware/'),
    
    # WiFi SSIDs
    (r'"GH Atas"', '"Your_Primary_SSID"'),
    (r'"GH Bawah"', '"Your_Secondary_SSID"'),
    (r"'GH Atas'", "'Your_Primary_SSID'"),
    (r"'GH Bawah'", "'Your_Secondary_SSID'"),
    
    # Passwords
    (r'"123von456"', '"Your_WiFi_Password"'),
    (r'"medini123"', '"admin123"'),
    
    # IP addresses (private ranges)
    (r'\b(?:192\.168|10\.0|172\.(?:1[6-9]|2\d|3[01]))\.\d{1,3}\.\d{1,3}\b', '192.168.1.100'),
    
    # Long hex strings (tokens, hashes) - only if 64+ chars (SHA256 hashes)
    (r'"[A-Fa-f0-9]{64}"', '"YOUR_HASHED_PASSWORD_HERE"'),
]

def should_copy(path: Path, base: Path) -> bool:
    """Check if path should be copied"""
    rel = path.relative_to(base)
    parts = rel.parts
    
    # Check excluded dirs
    for part in parts:
        if part in EXCLUDE_DIRS:
            return False
    
    # Check excluded files
    if path.name in EXCLUDE_FILES:
        return False
    
    return True

def sanitize_content(content: str) -> str:
    """Apply sanitization patterns to content"""
    for pattern, replacement in SANITIZE_PATTERNS:
        content = re.sub(pattern, replacement, content)
    return content

def main():
    print(f"Source: {SOURCE_DIR}")
    print(f"Target: {TARGET_DIR}")
    
    # Remove target if exists
    if TARGET_DIR.exists():
        print("Removing existing target directory...")
        shutil.rmtree(TARGET_DIR)
    
    # Create target
    TARGET_DIR.mkdir(parents=True)
    
    # Copy files
    copied = 0
    for src_path in SOURCE_DIR.rglob("*"):
        if not should_copy(src_path, SOURCE_DIR):
            continue
        
        rel_path = src_path.relative_to(SOURCE_DIR)
        dst_path = TARGET_DIR / rel_path
        
        if src_path.is_dir():
            dst_path.mkdir(parents=True, exist_ok=True)
        else:
            dst_path.parent.mkdir(parents=True, exist_ok=True)
            
            # Check if this is a template file
            template_key = str(rel_path).replace("\\", "/")
            if template_key in TEMPLATE_FILES:
                print(f"  [TEMPLATE] {rel_path}")
                dst_path.write_text(TEMPLATE_FILES[template_key], encoding="utf-8")
            elif src_path.suffix in [".cpp", ".h", ".hpp", ".ini", ".yml", ".yaml", ".json", ".md", ".txt"]:
                # Text file - copy with sanitization
                try:
                    content = src_path.read_text(encoding="utf-8")
                    sanitized = sanitize_content(content)
                    dst_path.write_text(sanitized, encoding="utf-8")
                except:
                    shutil.copy2(src_path, dst_path)
            else:
                # Binary file - copy as-is
                shutil.copy2(src_path, dst_path)
            
            copied += 1
    
    print(f"\nCopied {copied} files to {TARGET_DIR}")
    print("\n=== Template files created with placeholders ===")
    for f in TEMPLATE_FILES:
        print(f"  - {f}")
    
    print("\n=== Next steps ===")
    print("1. cd to the new directory")
    print("2. git init")
    print("3. git remote add origin <your-public-repo-url>")
    print("4. Review files before pushing!")

if __name__ == "__main__":
    main()
