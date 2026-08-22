#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$REPO_ROOT/build}"

if [[ "$BUILD_DIR" != /* ]]; then
    BUILD_DIR="$REPO_ROOT/$BUILD_DIR"
fi

mapfile -d '' TEST_BINARIES < <(
    find "$BUILD_DIR" -maxdepth 1 -type f -perm -111 \
        \( -name 'SharpRuntimeTests_*' -o -name 'SharpRuntimeIntegrationTests' \) \
        -print0 |
        sort -z
)

if [ "${#TEST_BINARIES[@]}" -eq 0 ]; then
    echo "FAIL: no component test binaries found in $BUILD_DIR" >&2
    exit 1
fi

mkdir -p "$REPO_ROOT/build-tmp"
TEST_LOG_DIR="$(TMPDIR="$REPO_ROOT/build-tmp" mktemp -d)"
trap 'rm -rf "$TEST_LOG_DIR"' EXIT

TOTAL_RUN=0
TOTAL_PASSED=0
TOTAL_FAILED=0
TOTAL_SKIPPED=0
for TEST_BINARY in "${TEST_BINARIES[@]}"; do
    TEST_NAME="$(basename "$TEST_BINARY")"
    TEST_LOG="$TEST_LOG_DIR/$TEST_NAME.log"

    TEST_EXIT_STATUS=0
    "$TEST_BINARY" >"$TEST_LOG" 2>&1 || TEST_EXIT_STATUS=$?

    if ! IFS=$'\t' read -r TEST_RUN TEST_PASSED TEST_FAILED TEST_SKIPPED < <(
        python3 "$REPO_ROOT/scripts/parse_gtest_summary.py" "$TEST_LOG"
    ); then
        echo "FAIL: could not read a consistent test summary from $TEST_NAME" >&2
        tail -40 "$TEST_LOG" >&2
        exit 1
    fi

    echo "    $TEST_NAME: $TEST_RUN run, $TEST_PASSED passed, $TEST_FAILED failed, $TEST_SKIPPED skipped"

    if [ "$TEST_EXIT_STATUS" -ne 0 ] || [ "$TEST_FAILED" -ne 0 ] || [ "$TEST_SKIPPED" -ne 0 ]; then
        echo "FAIL: $TEST_NAME is not a zero-failure/zero-skip result "\
             "(exit=$TEST_EXIT_STATUS, failed=$TEST_FAILED, skipped=$TEST_SKIPPED)" >&2
        grep -E "FAILED|\[  FAILED  \]" "$TEST_LOG" >&2 || true
        tail -40 "$TEST_LOG" >&2
        exit 1
    fi

    TOTAL_RUN=$((TOTAL_RUN + TEST_RUN))
    TOTAL_PASSED=$((TOTAL_PASSED + TEST_PASSED))
    TOTAL_FAILED=$((TOTAL_FAILED + TEST_FAILED))
    TOTAL_SKIPPED=$((TOTAL_SKIPPED + TEST_SKIPPED))
done

echo "    $TOTAL_RUN tests run across ${#TEST_BINARIES[@]} executables: $TOTAL_PASSED passed, $TOTAL_FAILED failed, $TOTAL_SKIPPED skipped"
