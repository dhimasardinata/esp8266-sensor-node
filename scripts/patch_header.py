import os
import gzip
import re

# Paths
PROJECT_DIR = r"C:\Users\ASUS\Documents\Dhimas\TA\node-medini"
DATA_FILE = os.path.join(PROJECT_DIR, "data", "portal.html")
HEADER_FILE = os.path.join(PROJECT_DIR, "include", "WebAppData.h")

def generate_cpp_array(name, data):
    decl = []
    
    # Header line to match existing format style exactly
    decl.append(f"const uint8_t {name}[] PROGMEM __attribute__((aligned(4))) = {{")
    
    line = "  "
    for i, byte in enumerate(data):
        line += f"0x{byte:02x}, "
        if (i + 1) % 16 == 0:
            decl.append(line)
            line = "  "
    if line.strip():
        decl.append(line.rstrip(", "))
    decl.append("};")
    
    # Metadata variables
    decl.append(f"const size_t {name}_LEN = {len(data)};")
    decl.append(f'const char {name}_MIME[] = "text/html";')
    decl.append(f"const bool {name}_GZIPPED = true;") 
    
    return "\n".join(decl)

def main():
    print(f"Reading {DATA_FILE}...")
    with open(DATA_FILE, "rb") as f:
        content = f.read()
    
    print(f"Original size: {len(content)} bytes")
    
    # Gzip compress
    compressed = gzip.compress(content, compresslevel=9)
    print(f"Compressed size: {len(compressed)} bytes")
    
    # Generate C++ block
    cpp_block = generate_cpp_array("PORTAL_HTML", compressed)
    
    print(f"Reading {HEADER_FILE}...")
    with open(HEADER_FILE, "r", encoding="utf-8") as f:
        header_content = f.read()
        
    # Find the start and end of the PORTAL_HTML block
    # We look for the start signature and the GZIPPED bool at the end
    start_marker = "const uint8_t PORTAL_HTML[] PROGMEM"
    end_marker = "const bool PORTAL_HTML_GZIPPED = false;" # Old one was false maybe? Or true?
    # Wait, the tool output said "const bool PORTAL_HTML_GZIPPED = false;" at line 3913.
    # But usually my regeneration sets it to true.
    # So I should regex search for the variable definition.
    
    # Robust Regex Replacement
    # Matches: const uint8_t PORTAL_HTML[] ... -> const bool PORTAL_HTML_GZIPPED = ...;
    pattern = re.compile(
        r'const uint8_t PORTAL_HTML\[\] PROGMEM.*?const bool PORTAL_HTML_GZIPPED\s*=\s*(true|false);',
        re.DOTALL
    )
    
    match = pattern.search(header_content)
    if not match:
        print("ERROR: Could not find PORTAL_HTML block in header file.")
        return
        
    print(f"Found block from index {match.start()} to {match.end()}")
    
    new_header = header_content[:match.start()] + cpp_block + header_content[match.end():]
    
    print(f"Writing to {HEADER_FILE}...")
    with open(HEADER_FILE, "w", encoding="utf-8") as f:
        f.write(new_header)
        
    print("Done.")

if __name__ == "__main__":
    main()
