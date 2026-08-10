# Audit: `modules/core/include/System/Int16.hpp`

## Metadata

- Audit status: AUDITED (355 lines, header-only implementation, full read).
- Validation: the focused 8/16-bit numeric filter passed 312/312 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-small-integer-audit-probe.cpp`, compiled
  against `build/libsharp_runtime_core.a`.

## Assessment

The header correctly separates parsing failures, safeguards `Abs` and
`CopySign` at `MinValue`, and uses `<bit>` rotations. Its generic-math
predicate, formatting fallbacks, and invalid Clamp interval handling repeat
the defects independently demonstrated in the smaller signed wrapper.

## Finding references

- **SR-AUD-021:** `ToString(short{5}, "Q")` returns `"5"` rather than
  throwing `System::FormatException`.
- **SR-AUD-022:** `Clamp(5, 10, 0)` passes inverted bounds to `std::clamp` and
  returns `0` in the probe instead of raising an argument error.
- **SR-AUD-023:** `ToString(short{5}, "B")` returns decimal `"5"` rather than
  integral binary text.
- **SR-AUD-024:** `IsPositive(0)` returns `false`, while .NET's generic-math
  definition uses `value >= 0`.

## Required post-audit verification

Add exact exceptions for `"Q"` and inverted Clamp bounds, binary vectors for
positive, zero, padded, and `MinValue` raw bits, and a positive-zero assertion
after the predicate repair.

## Final assessment

The signed-boundary arithmetic is intentionally hardened; four shared API
contract gaps remain observable and were not repaired in this phase.
