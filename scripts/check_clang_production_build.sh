#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

# Compile every production component with Clang and the repository warning policy, without
# paying for GoogleTest or any test translation unit. Ticket #37 established -Werror for both
# GCC and Clang, but the ordinary Linux build uses the default GCC and therefore cannot see
# Clang-only diagnostics.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

if [ "$#" -ne 0 ]; then
    echo "Usage: $0" >&2
    exit 2
fi

resolve_compiler() {
    local requested="$1"
    local fallback="$2"
    local label="$3"
    local candidate="${requested:-$fallback}"
    local resolved
    if ! resolved="$(command -v "$candidate")"; then
        echo "FAIL: $label compiler '$candidate' was not found" >&2
        exit 2
    fi
    printf '%s\n' "$resolved"
}

CLANG_C_COMPILER="$(resolve_compiler "${SHARP_RUNTIME_CLANG_C_COMPILER:-}" clang "Clang C")"
CLANG_CXX_COMPILER="$(
    resolve_compiler "${SHARP_RUNTIME_CLANG_CXX_COMPILER:-}" clang++ "Clang C++"
)"

# Use the repository-wide resolver instead of accepting CMake's or the generator's unbounded
# default. Values above two fail before a temporary tree or compiler process is created.
BUILD_JOBS="$(python3 "$REPO_ROOT/scripts/job_count_policy.py")"
export SHARP_RUNTIME_BUILD_JOBS="$BUILD_JOBS"

# A second compiler cannot safely reuse build/'s GCC cache. The gate therefore uses the one
# repository-local temporary root the build policy permits and removes the fresh Clang tree on
# every exit. It never creates a build under /tmp. Explicitly empty launchers also prevent an
# inherited CMake launcher setting from silently retrofitting ccache into this fresh tree.
mkdir -p "$REPO_ROOT/build-tmp"
GATE_ROOT="$(TMPDIR="$REPO_ROOT/build-tmp" mktemp -d)"
trap 'rm -rf "$GATE_ROOT"' EXIT

BUILD_DIR="$GATE_ROOT/build"
CONFIGURE_LOG="$GATE_ROOT/configure.log"
BUILD_LOG="$GATE_ROOT/build.log"

CMAKE_OPTIONS=(
    -S "$REPO_ROOT"
    -B "$BUILD_DIR"
    -DCMAKE_C_COMPILER="$CLANG_C_COMPILER"
    -DCMAKE_CXX_COMPILER="$CLANG_CXX_COMPILER"
    -DCMAKE_C_COMPILER_LAUNCHER=
    -DCMAKE_CXX_COMPILER_LAUNCHER=
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DSHARP_RUNTIME_COMPONENTS=All
    -DSHARP_RUNTIME_BUILD_TESTS=OFF
    -DSHARP_RUNTIME_BUILD_BENCHMARKS=OFF
)

CLANG_VERSION="$($CLANG_CXX_COMPILER --version)"
echo "==> Configuring Clang production-only build (${CLANG_VERSION%%$'\n'*})"
if ! cmake "${CMAKE_OPTIONS[@]}" >"$CONFIGURE_LOG" 2>&1; then
    echo "FAIL: Clang production configure failed" >&2
    tail -60 "$CONFIGURE_LOG" >&2
    exit 1
fi

# Do not merely trust the name of the gate. Every module implementation compile command must
# carry -Werror; otherwise a future CMake refactor could leave this build green while silently
# weakening the policy it exists to enforce. Vendor commands are intentionally outside this
# count because sharp_runtime_apply_build_options applies to first-party targets.
PRODUCTION_COMMANDS="$(
    grep -E '"command":.*[/]modules[/].*[/]src[/].*[.]cpp' \
        "$BUILD_DIR/compile_commands.json" || true
)"
if [ -z "$PRODUCTION_COMMANDS" ]; then
    echo "FAIL: Clang compile database contains no production module commands" >&2
    exit 1
fi
if printf '%s\n' "$PRODUCTION_COMMANDS" | grep -Fv -- ' -Werror ' >/dev/null; then
    echo "FAIL: a Clang production compile command does not carry -Werror" >&2
    printf '%s\n' "$PRODUCTION_COMMANDS" | grep -Fv -- ' -Werror ' >&2
    exit 1
fi

echo "==> Building every production component with Clang ($BUILD_JOBS job(s))"
if ! cmake --build "$BUILD_DIR" --parallel "$BUILD_JOBS" >"$BUILD_LOG" 2>&1; then
    echo "FAIL: Clang production build failed" >&2
    tail -80 "$BUILD_LOG" >&2
    exit 1
fi

WARNING_COUNT="$(grep -c 'warning:' "$BUILD_LOG" || true)"
ERROR_COUNT="$(grep -c 'error:' "$BUILD_LOG" || true)"
if [ "$WARNING_COUNT" -ne 0 ] || [ "$ERROR_COUNT" -ne 0 ]; then
    printf 'FAIL: Clang production build produced %s warning(s) and %s error(s)\n' \
        "$WARNING_COUNT" "$ERROR_COUNT" >&2
    grep -E 'warning:|error:' "$BUILD_LOG" >&2
    exit 1
fi

PRODUCTION_COMMAND_COUNT="$(printf '%s\n' "$PRODUCTION_COMMANDS" | wc -l)"
printf '    Clang production build clean: %s translation unit(s), 0 warnings, 0 errors\n' \
    "$PRODUCTION_COMMAND_COUNT"
