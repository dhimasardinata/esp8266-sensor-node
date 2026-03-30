
import os
import glob
import re

def analyze_stack_usage(pio_env, proj_dir):
    print("--- Analyzing Stack Usage ---")
    
    # Files are usually in .pio/build/<env>/src/
    search_path = os.path.join(proj_dir, ".pio", "build", pio_env, "**", "*.su")
    su_files = glob.glob(search_path, recursive=True)

    if not su_files:
        print(f"No .su files found in {search_path}")
        print("Make sure '-fstack-usage' is in your build_flags and you have compiled the project.")
        return

    # Parse .su files
    # Format: <file>:<line>:<column>:<function> <size> <type>
    functions = []
    
    for su_file in su_files:
        with open(su_file, 'r') as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) >= 3:
                     # parts[0] is often filepath:line:col:funcname, but sometimes spaced out depending on gcc version.
                     # Let's try to handle standard output.
                     # "main.cpp:25:5:int main()    16    static"
                     
                     full_def = " ".join(parts[:-2])
                     size_bytes = parts[-2]
                     func_type = parts[-1]
                     
                     try:
                         size = int(size_bytes)
                         functions.append({
                             'def': full_def,
                             'size': size,
                             'type': func_type,
                             'file': os.path.basename(su_file)
                         })
                     except ValueError:
                         continue

    # Sort by size descending
    functions.sort(key=lambda x: x['size'], reverse=True)

    print(f"\nTop 20 Stack Consumers:")
    print("-" * 80)
    print(f"{'Size (Bytes)':<12} | {'Type':<10} | {'Function Definition'}")
    print("-" * 80)
    
    for func in functions[:20]:
        print(f"{func['size']:<12} | {func['type']:<10} | {func['def']}")
        
    print("-" * 80)
    print(f"Total functions analyzed: REDACTED
    print("WARNING: This is static stack usage per frame. It does not calculate call graph depth.\n")

if __name__ == "__main__":
    # When run directly
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--env", default="wemosd1mini_usb", help="PIO Environment")
    parser.add_argument("--dir", default=".", help="Project Directory")
    args = parser.parse_args()
    
    analyze_stack_usage(args.env, args.dir)
    
else:
    # When run as a PIO script
    import subprocess
    try:
        Import("env") # type: ignore
        
        # We want to run this AFTER the build
        def run_stack_analysis(source, target, env):
            # Getting project dir from PIO env
            proj_dir = env.subst("$PROJECT_DIR")
            pio_env = env.subst("$PIOENV")
            analyze_stack_usage(pio_env, proj_dir)

        env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", run_stack_analysis)
            
    except ImportError:
        pass
