# Audit: `modules/core/tests/System/IntegerTypesTests.cpp`

## Metadata

- Audit status: AUDITED (113 lines, 16 tests, full read).
- Relevant validation: the `Int128Tests2.*` filter passed 3/3 on 2026-07-25.

## Assessment

This is a compact mixed-type smoke suite.  Its `Int128` portion verifies only
construction and `Abs` for small positive/negative values; the broader Int128
behavior is instead exercised in `Task40Tests.cpp`.  The test comments still
say “may be stub”, although the header is a substantial implementation with
public parse, format, arithmetic, and bit-operation behavior.

## Finding references

- **SR-AUD-019:** no test here or in the broader focused run supplies
  `Int128::MinValue` to decimal `ToString`, `Parse`, or `TryParse`; UBSan
  identifies signed negation UB on those paths.
- **SR-AUD-021:** no test requires invalid 128-bit numeric format strings to
  translate to `System::FormatException`.
- **SR-AUD-025:** the two IntPtr smoke tests do not exercise Add/Subtract at
  `MaxValue`/`MinValue`, where the implementation reaches UBSan-confirmed
  signed-overflow UB.

## Required post-audit verification

Move the Int128 comment from “may be stub” to an accurate coverage statement.
Add minimum-boundary parse/format vectors, invalid-format exception assertions,
and UBSan execution to the owning Int128 suite. Add native-boundary Add/Subtract
vectors to the owning IntPtr coverage; avoid duplicating every vector across
this mixed smoke file.

## Final assessment

The smoke checks pass but are far too narrow to characterize an implementation
of this size.  The missing extreme-value diagnostics are confirmed in the
owning source report, not repaired here.
