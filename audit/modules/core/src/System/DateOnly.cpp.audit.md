# Audit: `modules/core/src/System/DateOnly.cpp`

## Metadata

- Audit status: AUDITED (160-line implementation, fully read).
- Supporting validation: `DateOnlyTests.*:TimeOnlyTests.*` passed 119/119 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-dateonly-audit-probe.cpp`, compiled with
  `modules/core/src/System/DateOnly.cpp`, `-fsanitize=undefined`, and
  `-fno-sanitize-recover=undefined`, reports the four arithmetic failures
  below.  A non-sanitized run prints `1` for the trailing-text parse case.

## Assessment

The constructor deliberately delegates calendar validation to `DateTime`, and
ordinary JDN conversion, leap-year clamping, components, comparisons, and the
small documented format subset behave coherently on the existing cases.
However, range checks happen only indirectly, after intermediate signed `int`
arithmetic.  The parser uses `std::sscanf` without consuming the complete
input.  These are reachable public operations, not malformed internal state.

## SR-AUD-060 — high — DateOnly public arithmetic performs signed overflow before required range validation

`FromDayNumber(INT_MAX)` evaluates `dayNumber + JDN_EPOCH` at line 65;
`DateOnly::MaxValue.AddDays(INT_MAX)` adds the JDN at line 76;
`DateOnly(1,12,31).AddMonths(INT_MAX)` adds the month at line 81; and
`DateOnly(1,1,1).AddYears(INT_MAX / 12 + 1)` multiplies by twelve at line 92.
UBSan reports signed overflow at each site.  The latter two routes can also
enter enormous repeated month-normalization loops if execution continues after
undefined behavior.

Current local .NET `DateOnly.cs` validates `FromDayNumber` before conversion,
uses unsigned day-number arithmetic followed by a bound check in `AddDays`,
and delegates `AddMonths` / `AddYears` to `DateTime`, which rejects inputs
outside its bounded month/year domain before unsafe arithmetic.  The local C++
implementation must establish range validity or use defined wide/unsigned
intermediates before calculating, then report the project exception type; a
late `DateTime` constructor failure cannot repair UB already executed.

## SR-AUD-061 — medium — DateOnly parser accepts arbitrary trailing text after an ISO date

`TryParse` first checks only the positions of the two dashes and then accepts
`std::sscanf(..., "%d-%d-%d") == 3`.  `sscanf` reports success after a valid
prefix without requiring end-of-input.  The reproducer prints `1` for
`TryParse("2025-06-13-trailing", result)`, contradicting this API's documented
ISO date-string contract and the local .NET parser's complete-input parsing.
`Parse` inherits the false success.

The repair should replace prefix conversion with a full grammar/consumption
check, preserving the intentional ISO-only adaptation if that reduced surface
remains desired.

## Other missing assertions and diagnostics

- `FromDayNumber` relies on eventual DateTime construction instead of an
  explicit `0..MaxDayNumber` guard, so ordinary out-of-range calls have an
  indirect error origin and extreme calls have no safe diagnostic at all.
- `AddMonths` normalizes using repeated loops rather than quotient/remainder;
  even a non-overflow large input spends work proportional to the magnitude
  before it can report an invalid resulting year.
- `ToString(format)` treats unclosed quotes and unsupported/repeated tokens as
  loose literals or simplified tokens; it has no invalid-format diagnostic.
- The implementation deliberately supplies only an ISO parser/formatter,
  unlike .NET's culture/provider/standard/custom format breadth.  That is
  documented locally and is recorded as an adaptation boundary rather than a
  separate defect here.

## Final assessment

Ordinary date arithmetic is functional, but extreme public inputs reach
sanitizer-confirmed undefined behavior and parse accepts text outside the
claimed date grammar.  No source or test was modified during this audit.
