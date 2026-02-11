import os
import subprocess
import sys

# Extensions to format
EXTENSIONS = (".cpp", ".h", ".c", ".hpp", ".tpp")

# Directories to include
DIRS_TO_FORMAT = ["src", "include", "lib", "test"]


def format_file_smart(file_path):
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            original_content = f.read()

        # Run clang-format and capture output (no -i flag)
        # Use list-based command to avoid shell=True (Security Fix)
        cmd = ["clang-format", "-style=file", file_path]
        result = subprocess.run(
            cmd,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
        ) # nosec B603

        if result.returncode != 0:
            return False, f"Error: {result.stderr}"

        formatted_content = result.stdout

        if original_content != formatted_content:
            # Write only if changed
            with open(file_path, "w", encoding="utf-8") as f:
                f.write(formatted_content)
            return True, "Fixed"

        return False, "OK"

    except (IOError, OSError) as e:
        return False, str(e)


def collect_files(project_dir):
    file_list = []
    for folder in DIRS_TO_FORMAT:
        target_path = os.path.join(project_dir, folder)
        if not os.path.exists(target_path):
            continue
        for root, _, files in os.walk(target_path):
            for file in files:
                if file.endswith(EXTENSIONS):
                    file_list.append(os.path.join(root, file))
    return file_list


def main():
    # 1. Find Project Root
    try:
        from SCons.Script import DefaultEnvironment

        env = DefaultEnvironment()
        project_dir = env.subst("$PROJECT_DIR")
    except ImportError:
        script_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
        project_dir = os.path.abspath(os.path.join(script_dir, ".."))

    # 2. Check clang-format
    try:
        subprocess.run(
            ["clang-format", "--version"],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        ) # nosec B607
    except subprocess.CalledProcessError:
        print("[WARN] 'clang-format' not found. Skipping formatting.")
        return

    # 3. Collect Files
    print("--- Formatting Codebase... ---")
    all_files = collect_files(project_dir)
    total_files = REDACTED

    if total_files =REDACTED
        print("--- No source files found to format. ---")
        return

    # 4. Process with Progress Bar
    changed_count = 0

    for i, file_path in enumerate(all_files):
        changed, _ = format_file_smart(file_path)
        if changed:
            changed_count += 1
            # If changed, print a permanent line so we see what was fixed
            # \033[K clears the rest of the line (for VSCode/Terminal)
            print(f"\r[FIXED] {os.path.relpath(file_path, project_dir)} \033[K")

        # Update Progress Bar (Overwrites current line)
        percent = int(((i + 1) / total_files) * 100)
        filename = os.path.basename(file_path)
        # Pad filename to prevent jitter, limit length
        display_name = (
            (filename[:25] + "..") if len(filename) > 25 else filename.ljust(27)
        )

        sys.stdout.write(f"\r[{percent:>3}%] {display_name} ")
        sys.stdout.flush()

    # Final newline to clear the progress bar line
    sys.stdout.write(
        f"\r--- Formatting Complete. {changed_count} files updated. {' ' * 20}\n"
    )


if __name__ == "__main__":
    main()
else:
    try:
        from SCons.Script import DefaultEnvironment

        env = DefaultEnvironment()
        if not env.GetOption("clean"):
            main()
    except ImportError:
        pass
