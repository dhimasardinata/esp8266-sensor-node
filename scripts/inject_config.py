import os
import configparser
from SCons.Script import Import

Import("env")

# 1. BACA CONFIG DARI FILE EKSTERNAL
config = configparser.ConfigParser()
config.read("node_id.ini")

try:
    gh_id = config.get("settings", "gh_id")
    node_id = config.get("settings", "node_id")
    ota_ip = config.get("settings", "ota_ip", fallback="").strip()
except:
    print("!!! ERROR: File node_id.ini tidak ditemukan atau format salah !!!")
    gh_id = "0"
    node_id = "0"
    ota_ip = ""

hostname = f"gh-{gh_id}-node-{node_id}.local"

# 3. GENERATE HEADER FILE C++ (Tetap jalankan ini agar firmware tahu ID-nya)
header_content = f"""#ifndef DYNAMIC_NODE_CONFIG_H
#define DYNAMIC_NODE_CONFIG_H
// File ini digenerate otomatis oleh inject_config.py
// Edit 'node_id.ini' untuk mengubah nilai.
#define GH_ID {gh_id}
#define NODE_ID {node_id}
#endif
"""

output_file = os.path.join("include", "node_config.h")
needs_update = True

if os.path.exists(output_file):
    with open(output_file, "r") as f:
        if f.read().strip() == header_content.strip():
            needs_update = False

if needs_update:
    with open(output_file, "w") as f:
        f.write(header_content)
    print(f"   -> Config header updated: GH={gh_id}, Node={node_id}")
else:
    print(f"   -> Config header unchanged: GH={gh_id}, Node={node_id}")

# ------------------------------------------------------------------
# PERBAIKAN DI SINI: Cek Protokol Upload
# ------------------------------------------------------------------
upload_protocol = env.get("UPLOAD_PROTOCOL", "")

if upload_protocol == "espota":
    # Hanya ganti port jika modenya OTA
    print(f"\n--- [OTA MODE DETECTED] ---")
    # Logika pemilihan Target: IP Manual vs mDNS
    if ota_ip:
        print(f"   [OVERRIDE] Using Manual IP from node_id.ini: {ota_ip}")
        env.Replace(UPLOAD_PORT=ota_ip)
    else:
        print(f"   [AUTO] Using mDNS Hostname: {hostname}")
        env.Replace(UPLOAD_PORT=hostname)
        # env.Append(UPLOAD_FLAGS=["--auth=admin_password"]) # Jika pakai password
else:
    # Jika mode USB (esptool), biarkan PlatformIO mendeteksi COM port otomatis
    print(f"\n--- [USB MODE DETECTED] ---")
    print(f"   Upload Port: Auto-detect serial (Default)")
    # Jangan ubah UPLOAD_PORT
