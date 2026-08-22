#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

# Keeps the intentionally incremental documentation cleanup from regressing.
# The count was measured from the tracked Doxyfile with Doxygen 1.9.8 on
# 2026-08-20. Lower counts are improvements and remain valid.
set -euo pipefail

readonly EXPECTED_DOXYGEN_VERSION="1.9.8"
readonly MAX_WARNING_COUNT=2675

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$REPO_ROOT/build-tmp"
WARNING_LOG="$(TMPDIR="$REPO_ROOT/build-tmp" mktemp)"
trap 'rm -f "$WARNING_LOG"' EXIT

if ! command -v doxygen >/dev/null; then
    echo "FAIL: Doxygen ${EXPECTED_DOXYGEN_VERSION} is required for the warning baseline." >&2
    exit 1
fi

DOXYGEN_VERSION="$(doxygen --version)"
if [ "$DOXYGEN_VERSION" != "$EXPECTED_DOXYGEN_VERSION" ]; then
    echo "FAIL: expected Doxygen ${EXPECTED_DOXYGEN_VERSION}, found ${DOXYGEN_VERSION}." >&2
    echo "      Warning totals are version-sensitive; re-establish the baseline deliberately." >&2
    exit 1
fi

cd "$REPO_ROOT"
if ! doxygen Doxyfile >"$WARNING_LOG" 2>&1; then
    echo "FAIL: Doxygen did not complete successfully." >&2
    tail -60 "$WARNING_LOG" >&2
    exit 1
fi

WARNING_COUNT="$(grep -c ': warning:' "$WARNING_LOG" || true)"
if [ "$WARNING_COUNT" -gt "$MAX_WARNING_COUNT" ]; then
    echo "FAIL: Doxygen emitted ${WARNING_COUNT} warnings; baseline maximum is ${MAX_WARNING_COUNT}." >&2
    grep ': warning:' "$WARNING_LOG" >&2
    exit 1
fi

echo "Doxygen ${DOXYGEN_VERSION}: ${WARNING_COUNT} warning(s) (baseline maximum: ${MAX_WARNING_COUNT})"
