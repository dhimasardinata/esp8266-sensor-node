# File: extra_script.py

import sys
from pathlib import Path

Import("env") # noqa: F821

print("--- Running extra_script.py to customize build ---")

# ===========================================================
# ===========================================================
# PART 1: Separate C and C++ flags to eliminate warnings.
# ===========================================================
cpp_only_flags = ["-fno-rtti", "-fno-exceptions", "-Wno-volatile"]

# Retrieve all build flags.
all_flags = env.get("BUILD_FLAGS", []) # noqa: F821

# Create new list without C++ specific flags.
c_and_cpp_flags = [flag for flag in all_flags if flag not in cpp_only_flags]

# Replace BUILD_FLAGS with only common flags.
env.Replace(BUILD_FLAGS=c_and_cpp_flags) # noqa: F821

# Append C++ specific flags only to CXXFLAGS.
env.Append(CXXFLAGS=cpp_only_flags) # noqa: F821

print("Separated C/C++ flags successfully.")


def build_all_nodes_target(source, target, env_):  # noqa: ARG001
    repo_root = Path(env_.subst("$PROJECT_DIR"))
    script = repo_root / "scripts" / "build_all.py"
    command = [sys.executable, str(script), "--env", env_["PIOENV"]]
    return env_.Execute(" ".join(f'\"{part}\"' if " " in part else part for part in command))


env.AddCustomTarget(  # noqa: F821
    name="build_all_nodes",
    dependencies=None,
    actions=[build_all_nodes_target],
    title="Build All Nodes",
    description="Build node1.bin..node10.bin with automatic GH mapping",
)
