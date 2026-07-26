# Audit: `modules/core/include/System/UInt16.hpp`

## Metadata

- Audit status: AUDITED (285 lines, header-only implementation, full read).
- Validation: the focused 8/16-bit numeric filter passed 312/312 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-small-integer-audit-probe.cpp`, compiled
  against `build/libsharp_runtime_core.a`.

## Assessment

Unsigned parse handling explicitly rejects `-0`, trailing text, and range
overflow. Normal bit operations and rotations pass their available vectors.
The formatter accepts only `X`, `D`, and `G`, silently treats every other
letter as decimal, and `Clamp` does not validate its public interval.

## Finding references

- **SR-AUD-021:** `ToString(ushort{5}, "Q")` returns `"5"` rather than
  throwing `System::FormatException`.
- **SR-AUD-022:** `Clamp(5, 10, 0)` forwards invalid bounds to `std::clamp` and
  returns `0` in the direct probe.
- **SR-AUD-023:** `ToString(ushort{5}, "B")` silently returns decimal `"5"`
  despite the integral binary-format contract.

## Required post-audit verification

Add `FormatException` coverage for an unknown format, argument-error coverage
for `min > max`, and `B`/`b` vectors for zero, width, and `MaxValue`.

## Final assessment

Common parsing paths are carefully covered, but format validation, binary
format support, and invalid-range diagnostics remain incomplete.
