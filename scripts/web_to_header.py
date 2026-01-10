#!/usr/bin/env python3
import os
import re
import gzip
from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()

PROJECT_DIR = env.subst("$PROJECT_DIR")
DATA_DIR = os.path.join(PROJECT_DIR, "data")
INCLUDE_DIR = os.path.join(PROJECT_DIR, "include")
HEADER_FILE = os.path.join(INCLUDE_DIR, "WebAppData.h")


def make_valid_varname(path):
    name = os.path.basename(path)
    name = name.upper().replace(".", "_").replace("-", "_")
    name = "".join(c if (c.isalnum() or c == "_") else "_" for c in name)
    return name


def process_file(filepath):
    filename = os.path.basename(filepath)
    var_name = make_valid_varname(filename)
    _, ext = os.path.splitext(filename)
    ext = ext.lower()

    mime_map = {
        ".html": "text/html",
        ".css": "text/css",
        ".js": "application/javascript",
        ".json": "application/json",
        ".png": "image/png",
        ".jpg": "image/jpeg",
        ".ico": "image/x-icon",
    }
    mime = mime_map.get(ext, "application/octet-stream")

    try:
        with open(filepath, "rb") as f:
            data = f.read()
    except Exception as e:
        print(f"[ERROR] Reading {filename}: {e}")
        return ""

    is_template = False
    if ext == ".html":
        try:
            text_content = data.decode("utf-8")
            if re.search(r"%[A-Z0-9_]+%", text_content):
                is_template = True
                print(f"  |-- {filename}: Skipped compression (Template detected)")
        except:
            pass

    should_compress = (
        ext in [".html", ".css", ".js", ".json"] and len(data) > 100 and not is_template
    )

    out_data = data
    if should_compress:
        try:
            out_data = gzip.compress(data, compresslevel=9)
            print(f"  |-- {filename}: {len(data)} -> {len(out_data)} bytes saved")
        except Exception as e:
            should_compress = False

    decl = []
    decl.append(f"// File: {filename}")

    # 1. DATA: Tetap di PROGMEM dengan Alignment 4 Byte (Wajib untuk ESP8266)
    decl.append(f"const uint8_t {var_name}[] PROGMEM __attribute__((aligned(4))) = {{")

    line = "  "
    for i, byte in enumerate(out_data):
        line += f"0x{byte:02x}, "
        if (i + 1) % 16 == 0:
            decl.append(line)
            line = "  "
    if line.strip():
        decl.append(line.rstrip(", "))
    decl.append("};")

    decl.append(f"const size_t {var_name}_LEN = {len(out_data)};")

    # 2. MIME TYPE: Hapus 'PROGMEM'. Simpan di RAM agar aman dibaca library.
    #    Ini solusi anti-crash paling ampuh untuk string pendek.
    decl.append(f'const char {var_name}_MIME[] = "{mime}";')

    decl.append(
        f"const bool {var_name}_GZIPPED = {'true' if should_compress else 'false'};"
    )

    return "\n".join(decl) + "\n\n"


if not os.path.exists(INCLUDE_DIR):
    os.makedirs(INCLUDE_DIR)

print("--- Generating WebAppData.h (Aligned Safe Mode) ---")

try:
    with open(HEADER_FILE, "w", encoding="utf-8") as h:
        h.write("#ifndef WEB_APP_DATA_H\n")
        h.write("#define WEB_APP_DATA_H\n\n")
        h.write("#include <Arduino.h>\n\n")

        if os.path.isdir(DATA_DIR):
            files = sorted(os.listdir(DATA_DIR))
            for fn in files:
                fp = os.path.join(DATA_DIR, fn)
                if os.path.isfile(fp):
                    h.write(process_file(fp))

        h.write("#endif // WEB_APP_DATA_H\n")
    env.Append(CPPPATH=[INCLUDE_DIR])

except Exception as e:
    print(f"FATAL ERROR: {e}")
    env.Exit(1)
