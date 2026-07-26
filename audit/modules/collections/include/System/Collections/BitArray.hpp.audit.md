# Audit: `modules/collections/include/System/Collections/BitArray.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-364 — medium — BitArray enumerator exposes Current outside its valid lifecycle and ignores mutation

The enumerator stores only an integer cursor and a cached bool.  Before the first successful `MoveNext()` or after exhaustion, `getCurrentProperty()` still returns a pointer to the cache instead of throwing; no version state detects Set, SetAll, length, or bitwise mutation during iteration.  This violates the standard enumerator state and fail-fast contract even when it does not immediately cross a native buffer boundary.

## Missing assertions and diagnostics

- BitArray tests do not check Current before start/after end or mutation after enumerator creation.
- Record a version and expose an InvalidOperationException diagnostic for invalid state or mutation.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
