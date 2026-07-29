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
