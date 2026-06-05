from __future__ import annotations

import sys

from SCons.Script import Import


Import("env")

sys.path.insert(0, env["PROJECT_DIR"] + "/scripts")

from versioning import resolve_build_metadata  # noqa: E402


_, version, label = resolve_build_metadata()

env.Append(
    CPPDEFINES=[
        ("GH_BUILD_VERSION", env.StringifyMacro(version)),
        ("GH_BUILD_LABEL", env.StringifyMacro(label)),
    ]
)

print("Auto firmware version:", version, label)