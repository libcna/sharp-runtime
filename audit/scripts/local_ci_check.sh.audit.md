# Audit: `scripts/local_ci_check.sh`

## Metadata

- Audit status: AUDITED (44 lines, full read).
- Subsystem: local full validation gate.
- Evidence: script implementation and the audit's direct invocation.

## Purpose

Runs boundary validation, validator fixtures, catalogue freshness, configure,
warning-free build, and every component/integration test executable.

## Assessment

The gate fails fast and does not hide warning/error output.  Its build-log
counting is supplementary to CMake's exit status, and its test step delegates
to the component runner rather than duplicating execution logic.

The audit invocation passed all pre-test checks and built cleanly.  It then
failed only because this sandbox rejects socket creation for six local-server
Net.Http tests.  Project documentation already identifies local-network
permission as a prerequisite; the failure is preserved rather than skipped.

## Findings

No confirmed source defect.  The environment-specific failure is a validation
limitation recorded in `AUDIT_PROGRESS.md`; a future diagnostics improvement
could preflight local-network availability, but must not weaken the real tests.

## Final assessment

Correct strict gate.  A network-permitted final run remains required.

## Post-audit remediation note — ticket #1800 (2026-07-29)

The gate gained a fourth pre-build validation step, run before anything is
configured so a violation is reported in about a second rather than after a full
build:

```bash
echo "==> Validating test-only access seams (ticket #1800)"
python3 scripts/check_version_seam_odr.py
python3 test/check_version_seam_odr_test.py
```

`scripts/check_version_seam_odr.py` fails if a class template that a production
header declares and never defines inside `namespace SharpRuntime::Testing` — a
test-only access seam — is defined in more than one file, is defined
inconsistently, is defined in a production tree, or is written through two
different macros in one file. Five test translation units of one program had been
defining `CollectionVersionAccess` themselves in two divergent families, which is
ill-formed with no diagnostic required; §4 of
`docs/CollectionVersionTestSeamDesign.md` measures the consequence. The checker
needs no configured build and uses only the standard library, so it does not
weaken this script's fail-fast property or lengthen its critical path
measurably. `test/check_version_seam_odr_test.py` carries 12 fixtures for the
checker itself, in the same shape as the boundary validator's fixtures. The
assessment above is otherwise unchanged, including the local-network prerequisite.

## Post-audit remediation note — ticket #1801 (2026-07-29)

The gate gained a fifth pre-build validation step, immediately after #1800's seam
block and still before anything is configured:

```bash
echo "==> Validating negative consumer fixtures (ticket #1801)"
python3 scripts/check_negative_consumer_fixtures.py
python3 test/check_negative_consumer_fixtures_test.py
```

`scripts/check_negative_consumer_fixtures.py` fails if any marked negative site in
any `test/consumer/*_negative.cpp` compiles, if a fixture's all-sites-off baseline
does not compile cleanly, if a diagnostic located in a fixture falls outside the
enabled site, if a site's expected diagnostics no longer match, if a marker is
stale or duplicated, or if no fixture is discovered at all. Before this, seven
committed fixtures carrying 36 marked claims were compiled by **no tracked job**;
a whole-file check on a fixture with one site made legal was measured reporting a
false PASS, because nine other lines still failed. §3 of
`docs/NegativeConsumerFixtureValidation.md` records that reproduction.

Cost to this script's critical path: **≈ 15 s** — 44 `-fsyntax-only` compiles at
the mandatory three-job ceiling (12.5 s) plus 37 checker fixtures (2.1 s) — against
a gate whose build alone is ~346 s. The step needs no configured build directory
and no generated header: it reads the tracked sources and the CMake component
metadata as text, which is why it sits before `cmake -S . -B "$BUILD_DIR"` rather
than after it, preserving this script's fail-fast property. It uses only the
standard library. The checker refuses a `--jobs` value above three rather than
clamping it, so this script cannot become a route around the build-resource
policy. The assessment above is otherwise unchanged, including the local-network
prerequisite.
