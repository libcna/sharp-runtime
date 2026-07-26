# Audit: `modules/core/include/System/SByte.hpp`

## Metadata

- Audit status: AUDITED (394 lines, header-only implementation, full read).
- Validation: the focused 8/16-bit numeric filter passed 312/312 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-small-integer-audit-probe.cpp`, compiled
  against `build/libsharp_runtime_core.a`.

## Assessment

The wrapper makes careful choices around signed minimum magnitude, parse-end
validation, and raw-bit operations. Three externally visible parity defects
remain: unknown formats fall back to decimal, integral binary formatting is
absent, and invalid Clamp bounds reach `std::clamp`. It also implements the
generic-math `IsPositive` predicate with a strict comparison contrary to the
reference definition.

## SR-AUD-024 — medium — signed 8/16-bit `IsPositive(0)` contradicts the .NET generic-math contract

`SByte::IsPositive` returns `value > 0`; `Int16` has the identical condition.
The probe observed `sbyte_is_positive_zero=0` and
`int16_is_positive_zero=0`. The official SByte source defines the operation as
`value >= 0` (<https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/SByte.cs.html>), so zero must be
positive for this generic-math predicate. Both owning suites explicitly expect
the wrong result, preventing the normal green test run from detecting it.

### Required post-audit verification

Use `>= 0` in both signed wrappers and replace the zero assertions in both
owning suites with `EXPECT_TRUE`. Add an adjacent negative vector so the
positive/negative partition remains explicit.

## Finding references

- **SR-AUD-021:** `ToString(sbyte{5}, "Q")` returns `"5"` rather than
  throwing `System::FormatException`.
- **SR-AUD-022:** `Clamp(5, 10, 0)` reaches invalid `std::clamp` use and
  returns `0` in the direct probe.
- **SR-AUD-023:** `ToString(sbyte{5}, "B")` returns decimal `"5"`, although
  `B`/`b` are valid integral numeric formats.

## Final assessment

The minimum-value safeguards are strong, but the wrapper contains one direct
generic-math contract violation and three shared format/range discrepancies.
