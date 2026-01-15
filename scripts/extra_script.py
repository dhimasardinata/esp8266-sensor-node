# File: extra_script.py

Import("env") # noqa: F821

print("--- Running extra_script.py to customize build ---")

# ===========================================================
# BAGIAN 1: Memisahkan flag C dan C++ untuk menghilangkan peringatan
# ===========================================================
cpp_only_flags = ["-fno-rtti", "-fno-exceptions", "-Wno-volatile"]

# Ambil semua flag dari build_flags
all_flags = env.get("BUILD_FLAGS", []) # noqa: F821

# Buat daftar baru tanpa flag khusus C++
c_and_cpp_flags = [flag for flag in all_flags if flag not in cpp_only_flags]

# Ganti BUILD_FLAGS hanya dengan flag umum
env.Replace(BUILD_FLAGS=c_and_cpp_flags) # noqa: F821

# Tambahkan flag khusus C++ hanya ke CXXFLAGS
env.Append(CXXFLAGS=cpp_only_flags) # noqa: F821

print("Separated C/C++ flags successfully.")
