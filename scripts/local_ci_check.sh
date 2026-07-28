#!/usr/bin/env bash
# Runs the same checks CI would run: clean build with zero warnings/errors,
# then the full test suite. Exits non-zero on the first failure so it can be
# used as a pre-push gate. Mirrors CLAUDE.md's non-negotiable rules #1/#2.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${1:-build}"

echo "==> Validating module boundaries"
python3 scripts/validate_module_boundaries.py
python3 test/validate_module_boundaries_test.py
python3 scripts/generate_component_catalog.py --check

echo "==> Configuring ($BUILD_DIR)"
cmake -S . -B "$BUILD_DIR" >/dev/null

echo "==> Building (checking for zero warnings/errors)"
BUILD_LOG="$(mktemp)"
trap 'rm -f "$BUILD_LOG"' EXIT

if ! cmake --build "$BUILD_DIR" --parallel 3 > "$BUILD_LOG" 2>&1; then
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
