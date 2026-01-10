from pathlib import Path

Import("env", "projenv")

platform = env.PioPlatform()

# Direktori yang ingin kita bungkam peringatannya
SYSTEM_DIRS = [
    Path(platform.get_package_dir("framework-arduinoespressif8266")),
    Path(env.subst("$PROJECT_LIBDEPS_DIR")),
]

# Terapkan perubahan ke semua environment yang relevan
for e in (env, projenv, DefaultEnvironment()):
    system_includes = []
    user_includes = []

    # Pisahkan path include menjadi 'sistem' dan 'pengguna'
    if "CPPPATH" in e:
        for p in e["CPPPATH"]:
            p_path = Path(p)
            is_system = False
            for sys_dir in SYSTEM_DIRS:
                # Periksa apakah path 'p' berada di dalam salah satu direktori sistem
                try:
                    # Path.relative_to() akan error jika path tidak relatif
                    p_path.relative_to(sys_dir)
                    is_system = True
                    break
                except ValueError:
                    continue
            if is_system:
                system_includes.append(p)
            else:
                user_includes.append(p)

        # Ganti CPPPATH lama dengan HANYA include pengguna (-I)
        e.Replace(CPPPATH=user_includes)

        # Tambahkan include sistem sebagai flag -isystem ke CCFLAGS
        # (CCFLAGS berlaku untuk C dan C++)
        e.Append(CCFLAGS=[("-isystem", p) for p in system_includes])
