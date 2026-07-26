# Audit: `modules/core/tests/System/DateOnlyTimeOnlyTests.cpp`

## Metadata

- Audit status: AUDITED (353 lines, 52 tests, fully read: 37 DateOnly and 15
  supporting TimeOnly tests).
- Validation: `DateOnlyTests.*:TimeOnlyTests.*` passed 119/119 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.  The filter also runs the
  already-audited dedicated TimeOnly suite and five DateOnly aggregate smoke
  tests from a pending larger file.

## Assessment

The DateOnly portion exercises invalid calendar construction, normal leap-day
and cross-boundary arithmetic, DateTime conversion, basic ISO parsing,
selected formatting, Min/Max components, day numbers, weekday/year properties,
comparison, deconstruction, hashing, and conversion to DateTime.  The
supporting TimeOnly cases usefully exercise basic conversion/format routes, but
their dedicated source is audited separately.  Both sets are happy-path and
small-range dominant.

## Finding references

- **SR-AUD-060:** no test passes negative or out-of-range day numbers, an
  extreme signed `AddDays`/`AddMonths`/`AddYears` argument, or checks that the
  operation throws before doing unsafe arithmetic.  UBSan confirms all four
  public paths overflow.
- **SR-AUD-061:** invalid parse tests cover unrelated text and impossible
  dates but omit a syntactically valid ISO date followed by text; the probe
  proves that `2025-06-13-trailing` is incorrectly accepted.

## Other missing assertions and diagnostics

- No tests establish the result value after failed `TryParse`; current .NET
  specifies default/MinValue, whereas the local API leaves its existing output
  unchanged on several early failure paths.
- DateOnly formatting tests omit unsupported standard tokens, empty format,
  unclosed quotes, repeated `y`/`M`/`d` widths, and culture/provider behavior.
- The suite does not test `FromDayNumber(-1)`, `MaxDayNumber + 1`, Min/Max
  AddDays, large bounded/unbounded month/year inputs, or no-hang behavior.
- TimeOnly's supporting section omits exceptional `FromTimeSpan`, wrap/
  precision boundaries, and `ToString` invalid-format diagnostics; these
  remain supplementary evidence rather than a re-audit of its dedicated tests.

## Final assessment

Good ordinary state coverage does not exercise DateOnly's error-domain and
grammar boundaries, so all 119 filtered tests pass despite two confirmed
implementation defects.  No test was modified during this audit.
