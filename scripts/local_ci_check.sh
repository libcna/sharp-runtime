#!/usr/bin/env bash
# Runs the same checks CI would run: clean build with zero warnings/errors,
# then the full test suite. Exits non-zero on the first failure so it can be
# used as a pre-push gate. Mirrors CLAUDE.md's non-negotiable rules #1/#2.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${1:-build}"

# CLAUDE.md's build-resource policy caps every compilation in this repository at
# three parallel jobs and explicitly allows fewer. An explicit --parallel on the
# command line overrides CMAKE_BUILD_PARALLEL_LEVEL, so a hardcoded 3 could not
# be bounded from outside; SHARP_RUNTIME_BUILD_JOBS lets a caller lower the
# ceiling. It defaults to 3 and is rejected above 3, so the maximum is unchanged.
BUILD_JOBS="${SHARP_RUNTIME_BUILD_JOBS:-3}"
if ! [[ "$BUILD_JOBS" =~ ^[123]$ ]]; then
    echo "FAIL: SHARP_RUNTIME_BUILD_JOBS must be 1, 2 or 3 (got '$BUILD_JOBS')" >&2
    exit 1
fi

echo "==> Validating module boundaries"
python3 scripts/validate_module_boundaries.py
python3 test/validate_module_boundaries_test.py
python3 scripts/generate_component_catalog.py --check

echo "==> Validating test-only access seams (ticket #1800)"
python3 scripts/check_version_seam_odr.py
python3 test/check_version_seam_odr_test.py

# Per-site validation of the committed negative consumer fixtures (ticket
# #1801). It runs here, before anything is configured, because it needs only the
# tracked sources and the CMake component metadata -- and because a broken
# compile-rejection contract should be reported in seconds rather than after a
# full build. Its own compiles are capped at $BUILD_JOBS parallel jobs.
echo "==> Validating negative consumer fixtures (ticket #1801)"
python3 scripts/check_negative_consumer_fixtures.py --jobs "$BUILD_JOBS"
python3 test/check_negative_consumer_fixtures_test.py

echo "==> Configuring ($BUILD_DIR)"
cmake -S . -B "$BUILD_DIR" >/dev/null

echo "==> Building (checking for zero warnings/errors)"
BUILD_LOG="$(mktemp)"
trap 'rm -f "$BUILD_LOG"' EXIT

if ! cmake --build "$BUILD_DIR" --parallel "$BUILD_JOBS" > "$BUILD_LOG" 2>&1; then
    echo "FAIL: build failed" >&2
    tail -60 "$BUILD_LOG" >&2
    exit 1
fi

WARNING_COUNT="$(grep -c "warning:" "$BUILD_LOG" || true)"
ERROR_COUNT="$(grep -c "error:" "$BUILD_LOG" || true)"

if [ "$WARNING_COUNT" -ne 0 ] || [ "$ERROR_COUNT" -ne 0 ]; then
    echo "FAIL: build produced $WARNING_COUNT warning(s) and $ERROR_COUNT error(s)" >&2
    grep -E "warning:|error:" "$BUILD_LOG" >&2
    exit 1
fi
echo "    build clean: 0 warnings, 0 errors"

echo "==> Running tests"
scripts/run_component_tests.sh "$BUILD_DIR"

echo "==> Local CI check passed"
