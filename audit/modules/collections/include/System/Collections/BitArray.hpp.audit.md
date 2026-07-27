# Audit: `modules/collections/include/System/Collections/BitArray.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-364 — medium — BitArray enumerator exposes Current outside its valid lifecycle and ignores mutation

At audit time, the enumerator stored only an integer cursor and a cached bool.  Before the first successful `MoveNext()` or after exhaustion, `getCurrentProperty()` returned a pointer to the cache instead of throwing; no version state detected Set, SetAll, length, or bitwise mutation during iteration.  This violated the standard enumerator state and fail-fast contract even when it did not immediately cross a native buffer boundary.

## Missing assertions and diagnostics

- BitArray tests do not check Current before start/after end or mutation after enumerator creation.
- Record a version and expose an InvalidOperationException diagnostic for invalid state or mutation.

## Remediation

**REMEDIATED by ticket #1767 on 2026-07-27.** `BitArray` now uses the shared
lifecycle state for `Current`, captures a defined-width version, and increments
it after Set, SetAll, Length, all bitwise operations, Not, and both shifts.
`MoveNext` and `Reset` reject mutation with `InvalidOperationException`.
Permanent tests cover every mutator plus normal/before/after/Reset behavior;
the focused 13/13 suite, 1,435/1,435 Collections.Core target, direct
ASan/UBSan probe, and network-permitted 12,694-test gate pass.

## Final assessment

AUDITED. SR-AUD-364 was confirmed with reproducible evidence and is now
REMEDIATED; the original evidence is retained above.
