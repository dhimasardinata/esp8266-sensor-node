#!/usr/bin/env python3
"""
Sanitize Repository Script
Creates a COPY of the repository with all sensitive credentials replaced by placeholders.
Original repository remains unchanged.
Automatically initializes git, commits, and pushes to public repo.

Usage:
  python sanitize_for_public.py [--source SOURCE_DIR] [--target TARGET_DIR] [--remote REMOTE_URL]

Defaults can also be set via environment variables:
  SANITIZE_SOURCE_DIR, SANITIZE_TARGET_DIR, SANITIZE_REMOTE_URL
"""

import argparse
import os
import shutil
import re
import subprocess
import stat
import sys
from pathlib import Path
from datetime import datetime

# FIX: Configuration now uses environment variables with sensible defaults
# These can be overridden via command-line arguments
DEFAULT_SOURCE_DIR = Path(os.environ.get(
    "SANITIZE_SOURCE_DIR",
    Path(__file__).parent.parent  # Default: parent of scripts folder
))
DEFAULT_TARGET_DIR = Path(os.environ.get(
    "SANITIZE_TARGET_DIR",
    Path(__file__).parent.parent.parent / "esp8266-sensor-node"  # Sibling folder
))
DEFAULT_REMOTE_URL = os.environ.get(
    "SANITIZE_REMOTE_URL",
    "https://github.com/dhimasardinata/esp8266-sensor-node.git"
)

# Files to completely replace with templates
TEMPLATE_FILES = {
    "node_id.ini": """[settings]
gh_id = 1
node_id = 1
ota_ip = REDACTED
""",
    "include/calibration.h": '''// calibration.h
// Sensor calibration defaults (per-node).
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
EXCLUDE_DIRS = {".git", ".pio", "build", "node_modules", "__pycache__", ".vscode", ".gemini"}

# Files to exclude
EXCLUDE_FILES = {".env", ".env.local", "secrets.h"}

# Patterns to sanitize in remaining files
# Patterns to sanitize in remaining files
SANITIZE_PATTERNS = [
    # Project-specific URLs
    (r'atomic\.web\.id', 'example.com'),
    (r'https://atomic\.web\.id/api/sensor', 'https://example.com/api/sensor'),
    (r'https://atomic\.web\.id/api/get-file/', 'https://example.com/api/firmware/'),
    
    # WiFi SSIDs
    (r"REDACTED", "REDACTED"),
    (r"REDACTED", "REDACTED"),
    (r"REDACTED", "REDACTED"),
    (r"REDACTED", "REDACTED"),
    
    # Passwords
    (r"REDACTED", "REDACTED"),
    (r'"admin123"', '"admin123"'),
    
    # IP addresses (private ranges)
    (r'\b(?:192\.168|10\.0|172\.(?:1[6-9]|2\d|3[01]))\.\d{1,3}\.\d{1,3}\b', '192.168.1.100'),
    
    # Long hex strings (tokens, hashes) - only if 64+ chars (SHA256 hashes)
    (r"REDACTED", "REDACTED"),

    # Bearer REDACTED_TOKEN
    (r'(?i)(Authorization:REDACTED
    (r'(?i)\bBearer\s+[A-Za-z0-9\-\._~\+/]+=*\b', 'Bearer REDACTED_TOKEN'),
]

# Aggressive key-based redaction (used for INI/YAML/JSON/C/C++)
SENSITIVE_KEY_RE = re.compile(
    r"REDACTED"
    r"REDACTED",
    re.IGNORECASE,
)
SENSITIVE_LINE_RE = SENSITIVE_KEY_RE
KV_RE = re.compile(r"^(\s*[^#;\n]+?)(\s*[:=]\s*)(.+)$")
QUOTED_STRING_RE = re.compile(r'("([^"\\]|\\.)*"|\'([^\'\\]|\\.)*\')')
BUILD_FLAG_SENSITIVE_RE = re.compile(
    r'(-D\s*[A-Za-z0-9_]*(?:TOKEN|AUTH|PASS|PASSWORD|SSID|SECRET|KEY)[A-Za-z0-9_]*\s*=\s*)'
    r'("([^"\\]|\\.)*"|\'([^\'\\]|\\.)*\'|\S+)',
    re.IGNORECASE,
)


def force_remove_readonly(func, path, excinfo):
    """Error handler for shutil.rmtree to handle read-only files on Windows."""
    try:
        os.chmod(path, stat.S_IWRITE)
        func(path)
    except OSError as e:
        print(f"  [WARN] Could not remove {path}: {e}")


def aggressive_remove_dir(target: Path) -> bool:
    """Aggressively remove a directory, handling Windows permission issues."""
    if not target.exists():
        return True
    
    print(f"Removing existing target directory: {target}")
    
    # Method 1: Try shutil.rmtree with error handler
    try:
        # Use onexc for Python 3.12+, fallback to onerror for older versions
        import sys
        if sys.version_info >= (3, 12):
            shutil.rmtree(target, onexc=lambda fn, path, exc: force_remove_readonly(fn, path, exc))
        else:
            shutil.rmtree(target, onerror=force_remove_readonly)  # noqa: deprecated in 3.12
        if not target.exists():
            return True
    except OSError as e:
        print(f"  [INFO] shutil.rmtree failed: {e}")
    
    # Method 2: Try Windows rmdir /s /q command
    if sys.platform == "win32":
        try:
            print("  [INFO] Trying Windows rmdir...")
            subprocess.run(
                ["cmd", "/c", "rmdir", "/s", "/q", str(target)],
                capture_output=True,
                check=False,
                timeout=60
            ) # nosec B603
            if not target.exists():
                return True
        except (subprocess.TimeoutExpired, OSError) as e:
            print(f"  [INFO] rmdir failed: {e}")
    
    # Method 3: Try PowerShell Remove-Item
    if sys.platform == "win32":
        try:
            print("  [INFO] Trying PowerShell Remove-Item...")
            subprocess.run(
                ["powershell", "-Command", f"Remove-Item -Path '{target}' -Recurse -Force -ErrorAction SilentlyContinue"],
                capture_output=True,
                check=False,
                timeout=60
            ) # nosec B603
            if not target.exists():
                return True
        except (subprocess.TimeoutExpired, OSError) as e:
            print(f"  [INFO] PowerShell failed: {e}")
    
    if target.exists():
        print(f"  [ERROR] Could not remove {target}. Please close any programs using it and try again.")
        return False
    
    return True


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


def _redact_sensitive_line(line: str) -> str:
    """Redact sensitive values on a single line (keys, build_flags, and quoted strings)."""
    line_ending = ""
    raw = line
    if line.endswith("\r\n"):
        line_ending = "\r\n"
        raw = line[:-2]
    elif line.endswith("\n"):
        line_ending = "\n"
        raw = line[:-1]

    if not SENSITIVE_LINE_RE.search(raw):
        return line

    # Redact sensitive -D build flags (PlatformIO)
    raw = BUILD_FLAG_SENSITIVE_RE.sub(r'\1"REDACTED"', raw)

    m = KV_RE.match(raw)
    if not m:
        # Redact quoted secrets on sensitive lines
        if QUOTED_STRING_RE.search(raw):
            return QUOTED_STRING_RE.sub('"REDACTED"', raw) + line_ending
        return raw + line_ending

    key = m.group(1)
    sep = m.group(2)
    value = m.group(3)
    if not SENSITIVE_KEY_RE.search(key):
        return raw + line_ending

    # Preserve inline comments if present
    comment = ""
    comment_match = re.search(r"\s(//|#|;)", value)
    if comment_match:
        comment = value[comment_match.start():]
    return f"{key}{sep}REDACTED{comment}" + line_ending


def sanitize_content(content: str) -> str:
    """Apply sanitization patterns to content"""
    for pattern, replacement in SANITIZE_PATTERNS:
        content = re.sub(pattern, replacement, content)

    # Line-based redaction for any sensitive key patterns
    lines = content.splitlines(keepends=True)
    redacted = [ _redact_sensitive_line(line) for line in lines ]
    return "".join(redacted)


def run_git_command(cwd: Path, args: list, description: str) -> bool:
    """Run a git command and return success status."""
    try:
        print(f"  {description}...")
        # NOTE: args comes from hardcoded strings, not user input
        result = subprocess.run(
            ["git"] + args,  # nosec B603 - args are hardcoded strings from internal calls
            cwd=cwd,
            capture_output=True,
            text=True,
            check=False,
            timeout=120
        )
        if result.returncode != 0:
            print(f"    [WARN] {result.stderr.strip()}")
            return False
        return True
    except subprocess.TimeoutExpired:
        print("    [ERROR] Git command timed out")
        return False
    except OSError as e:
        print(f"    [ERROR] {e}")
        return False


def setup_git_and_push(target_dir: Path, remote_url: str) -> bool:
    """Initialize git repo, commit all files, and push to remote."""
    print("\n=== Setting up Git repository ===")
    
    # Check if .git already exists
    git_dir = target_dir / ".git"
    if git_dir.exists():
        print("  Git repo already exists, pulling latest...")
        run_git_command(target_dir, ["fetch", "origin"], "Fetching from origin")
    else:
        # Initialize new repo
        if not run_git_command(target_dir, ["init"], "Initializing git repo"):
            return False
        
        # Add remote
        if not run_git_command(target_dir, ["remote", "add", "origin", remote_url], "Adding remote"):
            # Remote might already exist, try setting it
            run_git_command(target_dir, ["remote", "set-url", "origin", remote_url], "Setting remote URL")
    
    # Configure git (in case not configured globally)
    run_git_command(target_dir, ["config", "user.email", "sanitize@local"], "Setting git email")
    run_git_command(target_dir, ["config", "user.name", "Sanitize Script"], "Setting git name")
    
    # Add all files
    if not run_git_command(target_dir, ["add", "-A"], "Staging all files"):
        return False
    
    # Commit
    commit_msg = f"Sanitized sync: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
    if not run_git_command(target_dir, ["commit", "-m", commit_msg, "--allow-empty"], "Creating commit"):
        print("    [INFO] Nothing to commit or commit failed")
    
    # Push (force to handle divergent histories)
    print("  Pushing to remote (this may take a moment)...")
    if not run_git_command(target_dir, ["push", "-u", "origin", "main", "--force"], "Pushing to main"):
        # Try master branch
        if not run_git_command(target_dir, ["push", "-u", "origin", "master", "--force"], "Pushing to master"):
            # Try creating and pushing to main
            run_git_command(target_dir, ["branch", "-M", "main"], "Renaming to main")
            if not run_git_command(target_dir, ["push", "-u", "origin", "main", "--force"], "Pushing to main"):
                print("    [ERROR] Push failed. Please check your git credentials and remote URL.")
                return False
    
    print("  [SUCCESS] Pushed to remote!")
    return True



def copy_and_sanitize_files(source_dir: Path, target_dir: Path) -> int:
    """Copy files from source to target, applying sanitization."""
    copied_count = 0
    error_count = 0
    
    print("\n=== Copying and sanitizing files ===")
    
    for src_path in source_dir.rglob("*"):
        if not should_copy(src_path, source_dir):
            continue
        
        rel_path = src_path.relative_to(source_dir)
        dst_path = target_dir / rel_path
        
        try:
            if src_path.is_dir():
                dst_path.mkdir(parents=True, exist_ok=True)
            else:
                dst_path.parent.mkdir(parents=True, exist_ok=True)
                
                # Check if this is a template file
                template_key = str(rel_path).replace("\\", "/")
                if template_key in TEMPLATE_FILES:
                    print(f"  [TEMPLATE] {rel_path}")
                    dst_path.write_text(TEMPLATE_FILES[template_key], encoding="utf-8")
                elif src_path.suffix in [".cpp", ".h", ".hpp", ".ini", ".yml", ".yaml", ".json", ".md", ".txt", ".html", ".css", ".js", ".py"]:
                    # Text file - copy with sanitization
                    try:
                        content = src_path.read_text(encoding="utf-8")
                        sanitized = sanitize_content(content)
                        dst_path.write_text(sanitized, encoding="utf-8")
                    except UnicodeDecodeError:
                        shutil.copy2(src_path, dst_path)
                else:
                    # Binary file - copy as-is
                    shutil.copy2(src_path, dst_path)
                
                copied_count += 1
        except OSError as e:
            print(f"  [ERROR] {rel_path}: {e}")
            error_count += 1
            
    if error_count > 0:
        print(f"[WARN] {error_count} files had errors")
        
    return copied_count

def main():
    # FIX: Parse command-line arguments for portability
    parser = argparse.ArgumentParser(
        description="Sanitize repository for public release",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Environment variables SANITIZE_SOURCE_DIR, SANITIZE_TARGET_DIR, "
               "SANITIZE_REMOTE_URL can also be used."
    )
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE_DIR,
                        help=f"Source directory (default: {DEFAULT_SOURCE_DIR})")
    parser.add_argument("--target", type=Path, default=DEFAULT_TARGET_DIR,
                        help=f"Target directory (default: {DEFAULT_TARGET_DIR})")
    parser.add_argument("--remote", type=str, default=DEFAULT_REMOTE_URL,
                        help=f"Git remote URL (default: {DEFAULT_REMOTE_URL})")
    args = parser.parse_args()
    
    source_dir = Path(args.source).resolve()
    target_dir = Path(args.target).resolve()
    remote_url = args.remote
    
    print(f"{'='*60}")
    print("ESP8266 Sensor Node - Sanitize for Public Release")
    print(f"{'='*60}")
    print(f"Source: {source_dir}")
    print(f"Target: {target_dir}")
    print(f"Remote: {remote_url}")
    print()
    
    # Validate source exists
    if not source_dir.exists():
        print(f"[ERROR] Source directory does not exist: {source_dir}")
        sys.exit(1)
    
    # Aggressively remove target if exists
    if not aggressive_remove_dir(target_dir):
        print("\n[ERROR] Failed to remove target directory. Aborting.")
        sys.exit(1)
    
    # Create target
    try:
        target_dir.mkdir(parents=True)
    except OSError as e:
        print(f"[ERROR] Failed to create target directory: {e}")
        sys.exit(1)
    
    # Copy files
    copied = copy_and_sanitize_files(source_dir, target_dir)
    
    print(f"\nCopied {copied} files to {target_dir}")
    
    print("\n=== Template files created with placeholders ===")
    for f in TEMPLATE_FILES:
        print(f"  - {f}")
    
    # Setup git and push
    if not setup_git_and_push(target_dir, remote_url):
        print("\n[WARN] Git operations failed, but files were copied successfully.")
        print("You can manually push later with:")
        print(f"  cd {target_dir}")
        print("  git push -u origin main --force")
    
    print(f"\n{'='*60}")
    print("[DONE] Sanitization complete!")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
