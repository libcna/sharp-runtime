# Audit: `tests/integration/Task42Tests.cpp`

## Metadata

- Audit status: AUDITED (1,646 lines, 253 tests in 30 suites, full read).
- Runtime evidence: the focused timer, core utility, numeric, app/domain,
  IO, JSON, delegate, generic-math, and keyed-collection filter passed all 253
  cases on 2026-07-25 (the timer subset took 692 ms).

## Coverage observed

The file includes substantial regression coverage for timer rescheduling,
`JsonElement` numeric parsing without lossy/undefined `double` conversion,
`KeyedCollection` index consistency, `UInt64` bit helpers, and Linux
AppDomain path resolution.  It explicitly documents and tests several intended
permanent stubs (`Type` predicates, AppDomain/GC/debugging surfaces) instead of
mistaking them for complete .NET implementations.

## Missing assertions and diagnostics

- Timer behavior is established through sleeps from 20 to 150 ms.  This made
  the focused run pass, but it has scheduler-load flake risk and cannot precisely
  diagnose callback-in-flight/`Dispose` races.  Future tests should coordinate
  callback entry/exit with promises or condition variables and run TSan for
  the shared-state paths.
- `FiresCallbackBeforeDispose` sleeps after destruction but has no post-destruction
  assertion; it only establishes that the process did not visibly fail.  A
  lifetime regression needs a bounded completion signal or sanitizer evidence.
- String/Byte/UInt64 cover selected utility behavior but not whitespace/sign/
  overflow grammar, invalid format specifiers, culture, rotate counts outside
  word width, or division-by-zero diagnostics.
- Non-cryptographic hash tests assert lengths and reset differences, not known
  CRC32/xxHash vectors, chunking equivalence, seeds, or empty-input values.
- The direct `JsonElement` suite is strong for integer parsing, but lacks
  duplicate-property policy, nested traversal, escaped Unicode, `GetUInt*`,
  decimal, enumeration lifetime, and parser error locations.
- Tests that assert fixed GC/debugger/AppDomain/Type stub values must remain
  labeled as intended deviations.  They are not evidence of full framework
  compatibility; `Type.hpp` documents the predicate limitation explicitly.

## Required post-audit verification

Retain the existing timer and JSON integer regression cases.  Convert the
timing-sensitive timer cases to event-coordinated assertions where possible,
add known hash vectors/chunking tests, and add parser/format boundaries for the
numeric helpers.  Re-run timer lifetime cases under TSan rather than extending
unbounded sleeps.

## Final assessment

This is broad, mostly meaningful integration evidence with especially valuable
timer/JSON/keyed-collection regressions.  It identifies test-quality work but
does not demonstrate a new source defect beyond already documented deliberate
stubs.
