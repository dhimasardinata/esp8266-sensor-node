import os
import shutil

SOURCE_DIR = '.'
DEST_DIR = 'prompt'

ALLOWED_EXTENSIONS = {
    '.txt', '.json', '.h', '.cpp', '.py', '.md', 
    '.html', '.css', '.js'
}

IGNORED_DIRS = {
    '.pio', '.vscode', '.github', '.git', '__pycache__', 'prompt', 'node_modules', '.devcontainer', 'log'
}

SPECIFIC_EXCLUDES = {
    'WebAppData.h'
}

SPECIFIC_INCLUDES = {
    '.gitignore'
}

def is_allowed(filename):
    # exact ignores
    if filename.lower() in [f.lower() for f in SPECIFIC_EXCLUDES]:
        return False
    
    # exact includes
    if filename in SPECIFIC_INCLUDES:
        return True

    # Check for .yml vs .yml.txt
    # Files ending in .yml are excluded. Files ending in .yml.txt end in .txt.
    if filename.endswith('.yml'):
        return False
    
    # Check extension
    _, ext = os.path.splitext(filename)
    if ext in ALLOWED_EXTENSIONS:
        return True
    
    # Explicitly check for .yml.txt if not caught by .txt (though it should be)
    if filename.endswith('.yml.txt'):
        return True

    return False

def collect_files():
    if os.path.exists(DEST_DIR):
        # clear it first? Optional. Let's just create if not exists or use existing.
        pass
    else:
        os.makedirs(DEST_DIR)

    print(f"Collecting files from {os.path.abspath(SOURCE_DIR)} to {os.path.abspath(DEST_DIR)}...")

    count = 0
    for root, dirs, files in os.walk(SOURCE_DIR):
        # Filter directories in-place to prevent walking ignored dirs
        dirs[:] = [d for d in dirs if d not in IGNORED_DIRS]
        
        for file in files:
            if is_allowed(file):
                src_path = os.path.join(root, file)
                
                # Flattening: Create a unique name based on path to prevent collisions
                # e.g., lib/GreenhouseCommon/utils.h -> lib_GreenhouseCommon_utils.h
                rel_path = os.path.relpath(src_path, SOURCE_DIR)
                
                # Handle files in root logic (remove ./ prefix if present)
                if rel_path.startswith('.' + os.sep):
                    rel_path = rel_path[2:]
                
                safe_name = rel_path.replace(os.sep, '_')
                
                dest_path = os.path.join(DEST_DIR, safe_name)
                
                try:
                    shutil.copy2(src_path, dest_path)
                    print(f"Copied: {rel_path} -> {safe_name}")
                    count += 1
                except Exception as e:
                    print(f"Error copying {src_path}: {e}")

    print(f"Done. Copied {count} files.")

if __name__ == '__main__':
    collect_files()
