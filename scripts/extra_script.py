# File: extra_script.py

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
