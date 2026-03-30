from pathlib import Path

Import("env", "projenv") # noqa: F821

platform = env.PioPlatform()

# Directories to suppress warnings for.
SYSTEM_DIRS = [
    Path(platform.get_package_dir("framework-arduinoespressif8266")),
    Path(env.subst("$PROJECT_LIBDEPS_DIR")),
]

# Apply changes to all relevant environments.
for e in (env, projenv, DefaultEnvironment()): # noqa: F821
    system_includes = []
    user_includes = []

    # Separate include paths into 'system' and 'user'.
    if "CPPPATH" in e:
        for p in e["CPPPATH"]:
            p_path = Path(p)
            is_system = False
            for sys_dir in SYSTEM_DIRS:
                # Check if path 'p' is inside a system directory.
                try:
                    # Path.relative_to() raises error if path is not relative.
                    p_path.relative_to(sys_dir)
                    is_system = True
                    break
                except ValueError:
                    continue
            if is_system:
                system_includes.append(p)
            else:
                user_includes.append(p)

        # Replace CPPPATH with ONLY user includes (-I).
        e.Replace(CPPPATH=user_includes)

        # Add system includes as -isystem flags to CCFLAGS.
        # (CCFLAGS applies to both C and C++).
        e.Append(CCFLAGS=[("-isystem", p) for p in system_includes])
