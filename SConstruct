#!/usr/bin/env python
import os
import sys

env = SConscript("godot-cpp/SConstruct")

# For reference:
# - CCFLAGS are compilation flags shared between C and C++
# - CFLAGS are for C-specific compilation flags
# - CXXFLAGS are for C++-specific compilation flags
# - CPPFLAGS are for pre-processor flags
# - CPPDEFINES are for pre-processor defines
# - LINKFLAGS are for linking flags

env.Append(CPPPATH=["src/", "flecs/distr/"])
flecs_c_source = "flecs/distr/flecs.c"
sources = Glob("src/*.cpp")

# Always compile flecs.c as C code, and fix winsock header redefinition on Windows
if env["platform"] == "windows":
    env.Append(CXXFLAGS=["/std:c++17"])
    flecs_c_obj = env.SharedObject(
        target="flecs_c_obj",
        source=[flecs_c_source],
        # Only define WIN32_LEAN_AND_MEAN and force C compilation
        CCFLAGS=["/TC", "/DWIN32_LEAN_AND_MEAN"],
    )
else:
    env.Append(CXXFLAGS=["-std=c++17"])
    flecs_c_obj = env.SharedObject(
        target="flecs_c_obj",
        source=[flecs_c_source],
        CFLAGS=["-std=gnu99"],
    )

all_objs = env.SharedObject(sources) + [flecs_c_obj]

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "demo/bin/libflecs.{}.{}.framework/libflecs.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=all_objs,
    )
elif env["platform"] == "ios":
    if env["ios_simulator"]:
        library = env.StaticLibrary(
            "demo/bin/libflecs.{}.{}.simulator.a".format(env["platform"], env["target"]),
            source=all_objs,
        )
    else:
        library = env.StaticLibrary(
            "demo/bin/libflecs.{}.{}.a".format(env["platform"], env["target"]),
            source=all_objs,
        )
else:
    library = env.SharedLibrary(
        "demo/bin/libflecs{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=all_objs,
    )

Default(library)
