#!/usr/bin/env bash
# Runs the same checks CI would run: clean build with zero warnings/errors,
# then the full test suite. Exits non-zero on the first failure so it can be
# used as a pre-push gate. Mirrors CLAUDE.md's non-negotiable rules #1/#2.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
mkdir -p "$REPO_ROOT/build-tmp"

BUILD_DIR="${1:-build}"

# One shared resolver owns argument/environment/default precedence and the hard
# ceiling. Export the resolved value so every nested compiler-launching helper
# receives the same budget rather than independently expanding it.
BUILD_JOBS="$(python3 "$REPO_ROOT/scripts/job_count_policy.py")"
export SHARP_RUNTIME_BUILD_JOBS="$BUILD_JOBS"

echo "==> Validating module boundaries"
python3 scripts/validate_module_boundaries.py
python3 test/validate_module_boundaries_test.py
python3 scripts/generate_component_catalog.py --check

echo "==> Validating audit disposition metadata"
python3 scripts/reconcile_audit_findings.py
python3 scripts/validate_audit_findings.py
python3 test/validate_audit_findings_test.py

echo "==> Validating planning database consistency"
python3 scripts/db_consistency_check.py

echo "==> Validating local/GitHub selective-component matrix parity"
python3 scripts/validate_selective_component_matrix.py
python3 test/validate_selective_component_matrix_test.py

echo "==> Validating safe .NET type-index output handling"
python3 test/index_dotnet_types_test.py

echo "==> Validating source/planning inventory cross-reference"
python3 test/source_header_inventory_test.py
python3 scripts/source_header_inventory.py \
    --csv "$REPO_ROOT/build-tmp/source_header_inventory.csv"

echo "==> Validating committed Unicode tables (offline, exact when SA-4 source is available)"
python3 scripts/gen_unicode_tables.py --verify
python3 test/gen_unicode_tables_test.py

echo "==> Validating GoogleTest summary accounting"
python3 test/parse_gtest_summary_test.py

echo "==> Validating repository-local temporary paths"
python3 test/temporary_path_policy_test.py

echo "==> Validating the Clang production warning gate"
python3 test/check_clang_production_build_test.py

# Ticket #37 established -Werror as a GCC/Clang contract, but the ordinary Linux build below
# uses the configured/default GCC. Compile the complete production graph without tests so a
# Clang-only diagnostic cannot survive behind that otherwise-green GCC gate.
echo "==> Checking Clang production warnings"
scripts/check_clang_production_build.sh

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
BUILD_LOG="$(TMPDIR="$REPO_ROOT/build-tmp" mktemp)"
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

echo "==> Checking Doxygen warnings"
scripts/check_doxygen_warnings.sh

# ADDED BY #2415, AND THE REASON IS THAT ITS ABSENCE WAS THE DEFECT. The selective-component
# check verifies an invariant nothing else does -- that a component built on its own drags in no
# more than it declares -- and because NOTHING RAN IT, it sat red for a day behind a green test
# count: #1889 legitimately gave Text.Json a public Collections.Core dependency while a fixture
# still asserted the opposite, and CLAUDE.md rule 2's gate says nothing about either.
#
# IT COSTS ABOUT TEN MINUTES (measured 2026-08-20, two jobs), because it configures and builds one
# selective tree per matrix entry. That is stated here rather than left as a surprise. It runs
# LAST, so the cheap checks and the full suite still report first, and it is deliberately NOT
# behind an opt-out: a check that can be skipped is the check that rotted.
echo "==> Checking selective component isolation (~10 min)"
scripts/check_selective_components.sh

echo "==> Local CI check passed"
