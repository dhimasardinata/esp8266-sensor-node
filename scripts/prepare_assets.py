# File: scripts/prepare_assets.py

import os
import shutil
from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()
PROJECT_DIR = env.subst("$PROJECT_DIR")
NODE_MODULES = os.path.join(PROJECT_DIR, "node_modules")
DATA_DIR = os.path.join(PROJECT_DIR, "data")

print("--- Running prepare_assets.py ---")

# 1. Define Source and Destination
crypto_src = os.path.join(NODE_MODULES, "crypto-js", "crypto-js.js")
crypto_dst = os.path.join(DATA_DIR, "crypto.js")

# 2. Check if Node modules exist
if not os.path.exists(crypto_src):
    print(f"[WARN] Source file not found: {crypto_src}")
    print("       Did you run 'npm install'?")
    # We don't exit with error here to allow builds if the user
    # manually placed crypto.js in data/ previously.
else:
    try:
        # 3. Copy and Rename
        print(f"Copying {crypto_src} -> {crypto_dst}...")
        shutil.copyfile(crypto_src, crypto_dst)
        print("CryptoJS bundle updated successfully.")

        # 4. Cleanup old/partial files to save Flash memory
        # We only want crypto.js, not aes.js or core.js separately
        for old_file in ["aes.js", "core.js"]:
            old_path = os.path.join(DATA_DIR, old_file)
            if os.path.exists(old_path):
                print(f"Removing obsolete file: {old_file}")
                os.remove(old_path)

    except OSError as e:
        print(f"[ERROR] Failed to copy crypto libraries: {e}")
        env.Exit(1)
