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

TEST_LOG_DIR="$(mktemp -d)"
trap 'rm -rf "$TEST_LOG_DIR"' EXIT

TOTAL_TESTS=0
for TEST_BINARY in "${TEST_BINARIES[@]}"; do
    TEST_NAME="$(basename "$TEST_BINARY")"
    TEST_LOG="$TEST_LOG_DIR/$TEST_NAME.log"

    if ! "$TEST_BINARY" >"$TEST_LOG" 2>&1; then
        echo "FAIL: $TEST_NAME failed" >&2
        grep -E "FAILED|\[  FAILED  \]" "$TEST_LOG" >&2 || true
        tail -40 "$TEST_LOG" >&2
        exit 1
    fi

    SUMMARY_LINE="$(grep -E '^\[==========\] [0-9]+ tests? from .* ran\.' "$TEST_LOG" | tail -1)"
    if [ -z "$SUMMARY_LINE" ]; then
        echo "FAIL: could not read the test count from $TEST_NAME" >&2
        tail -40 "$TEST_LOG" >&2
        exit 1
    fi

    TEST_COUNT="$(sed -E 's/^\[==========\] ([0-9]+) tests? from .*/\1/' <<<"$SUMMARY_LINE")"
    TOTAL_TESTS=$((TOTAL_TESTS + TEST_COUNT))
    echo "    $TEST_NAME: $TEST_COUNT passed"
done

echo "    $TOTAL_TESTS tests passed across ${#TEST_BINARIES[@]} executables"
