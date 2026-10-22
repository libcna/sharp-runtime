#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Install, relocate and exercise an already-built Sharp Runtime package.

Uses only the standard build-consumer tree, at one compilation job. A fresh,
explicit staging path under build-probe is required; never removes another run.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", choices=("build", "build-modular"))
    parser.add_argument("--mode", choices=("full", "core"), required=True)
    parser.add_argument("--stage", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    build = root / args.build
    stage = args.stage.resolve()
    relocated = stage.with_name(stage.name + "-relocated")
    if stage.parent != root / "build-probe" or stage.exists() or relocated.exists():
        parser.error("--stage must be a fresh direct child of build-probe (including its -relocated sibling)")
    env = dict(os.environ, CMAKE_BUILD_PARALLEL_LEVEL="1", SHARP_RUNTIME_BUILD_JOBS="1")

    def run(command, *, failure=None, summary=False):
        result = subprocess.run(command, cwd=root, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
        if summary and result.returncode == 0:
            print(f"PASS: {command[0:2]} ({len(result.stdout.splitlines())} log lines)", flush=True)
        else:
            print(result.stdout, end="", flush=True)
        if failure:
            if result.returncode == 0 or failure not in result.stdout:
                raise RuntimeError(f"Expected diagnostic {failure!r}: {command}")
        elif result.returncode:
            raise RuntimeError(f"Command failed ({result.returncode}): {command}")

    run(["cmake", "--install", str(build), "--component", "SharpRuntime", "--prefix", str(stage)], summary=True)
    stage.rename(relocated)
    if not list(relocated.rglob("sharp-runtime/LICENSE")):
        raise RuntimeError("Project/.NET licence notices missing from installation")
    if args.mode == "full" and not list(relocated.rglob("vendor/miniz/miniz_zip.c")):
        raise RuntimeError("Private miniz copyright/permission notices missing")
    configs = list(relocated.rglob("*.cmake"))
    if not configs:
        raise RuntimeError("No package configuration installed")
    for config in configs:
        text = config.read_text()
        for forbidden in (str(root), str(stage)):
            if forbidden in text:
                raise RuntimeError(f"Non-relocatable path {forbidden} in {config}")
    package_configs = list(relocated.rglob("SharpRuntimeConfig.cmake"))
    if len(package_configs) != 1:
        raise RuntimeError("Expected exactly one installed SharpRuntimeConfig.cmake")
    package_dir = package_configs[0].parent
    configure = ["cmake", "-S", str(root / "test/installed"), "-B", str(root / "build-consumer"),
                 f"-DSharpRuntime_DIR={package_dir}", f"-DCMAKE_PREFIX_PATH={relocated}",
                 f"-DSR_TEST_FULL_PACKAGE={'ON' if args.mode == 'full' else 'OFF'}"]
    if shutil.which("ccache"):
        configure.append("-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
    # A source-tree embedding is a separate configure-only compatibility check,
    # never a fallback for the installed consumer tests below.
    run(configure + ["-DSR_TEST_SUBDIRECTORY=ON", "-DSR_TEST_FULL_PACKAGE=OFF"])
    configure.append("-DSR_TEST_SUBDIRECTORY=OFF")
    run(configure + ["-DSR_FIND_VERSION=0.1", "-DSR_REQUIRED_COMPONENT=Does.Not.Exist"],
        failure="Component 'Does.Not.Exist' is not present")
    run(configure + ["-DSR_FIND_VERSION=99.0", "-DSR_REQUIRED_COMPONENT=Core.Base"],
        failure='compatible with requested version "99.0"')
    if args.mode == "core":
        run(configure + ["-DSR_FIND_VERSION=0.1", "-DSR_REQUIRED_COMPONENT=Xml"],
            failure="Component 'Xml' is not present")
    run(configure + ["-DSR_FIND_VERSION=0.1", "-DSR_REQUIRED_COMPONENT=Core.Base"])
    run(["cmake", "--build", str(root / "build-consumer"), "--parallel", "1"])
    run(["ctest", "--test-dir", str(root / "build-consumer"), "--output-on-failure", "-j1"])
    if stage.exists():
        raise RuntimeError("Original installation must remain absent throughout consumer validation")
    print(f"PASS: {args.mode} installed package relocated to {relocated}; max jobs=1")


if __name__ == "__main__":
    main()
