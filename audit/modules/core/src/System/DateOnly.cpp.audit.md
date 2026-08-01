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

**Remediated (#1879, 2026-07-31, CCF-002 class D).** Approved in the exact words
of `docs/RemainingApprovalDecisions.md` §C.8 item (1). `std::sscanf` was replaced
by the full-consumption `System::detail::DateTimeTextScanner`, so the parser
consumes the whole string or fails, and the `%d` leniencies that came with
`sscanf` (leading whitespace, an explicit sign, an out-of-`int` numeral) are gone
with it. Every well-formed input keeps its exact previous value. Measured over 82
cases in `build-probe/1879_{prefix,postfix}_plain.log`; +29 permanent tests
across the four parsers. No signature, `noexcept`, layout, vtable or symbol
change. Two of the fifteen approved rejections rest on an incorrect .NET premise
and are recorded, not silently reversed, in
`docs/DateTimeValidationBoundaryPlan.md` §20.3.1 (inactive ticket #1929).

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

---

**REMEDIATED 2026-07-30 (ticket #1837, CCF-004 class C).** The text above is retained exactly
as the audit wrote it. Corrections belong beside it, all by measurement
(`build-probe/1837_dateonly_surface.cpp`, `build-probe/1837_prefix.log` vs
`build-probe/1837_postfix.log`, one process per case against an instrumented `build-asan` tree
proven newer than the source before **and** after the edit; recorded in full as
`docs/DefinedArithmeticBoundaryPlan.md` section 18):

1. **The finding's site count is seven, not four.** The audit named four entry-point sites
   (`:65` `FromDayNumber`, `:76` `AddDays`, `:81` `AddMonths`, `:92` `AddYears`). Overflow at the
   first two **cascades** into `jdnToDate`, which overflows three more times per call at `:35`,
   `:37`, `:39`. Guarding only the four entry points would have left the cascade reachable, so
   `jdnToDate` had to be made **provably unreachable with an out-of-range argument** — which the
   repair does by range-checking the day number before the conversion.
2. **The cascade is reachable in the negative direction WITHOUT an entry-point overflow.**
   `FromDayNumber(INTCS_MIN)` and `MinValue.AddDays(INTCS_MIN)` never overflow at `:65`/`:76`
   (the epoch addition stays in range) but drive `:35`/`:37`/`:39` directly with a wildly
   out-of-range value. This is a separate requirement, not a restatement of the first.
3. **Two of the rejected inputs were silent WRONG ANSWERS, not undefined behaviour.**
   `DateOnly(1,1,1).AddYears(INTCS_MIN)` computed `INTCS_MIN*12 == -6*2^32`, which wrapped to
   **zero** and returned `0001-01-01` unchanged — a request for a date 2.1 billion years earlier
   answered with the same date and no error. A UBSan sweep enumerates undefined operations, not
   wrong answers; this one it does report (the multiply overflows) but the *return* is the defect.
4. **`AddMonths`'s normalisation loop is load-bearing.** `AddMonths(INTCS_MIN)` did not overflow
   `month_ + n` (`1 + INT_MIN` fits) but ran its `while (m < 1)` loop about **179 million** times
   before returning. The repair replaces both loops with .NET's single-division normalisation and
   bounds the delta to +/-120000 first, so no path can iterate unboundedly.

The repair mirrors .NET exactly: `FromDayNumber`/`AddDays` do day-number arithmetic in the
unsigned domain with one unsigned range compare (`DateOnly.cs:73-81`, `:121-132`); `AddMonths`
bounds the delta and normalises by division (`DateTime.cs:960-977`); `AddYears` bounds the delta
to +/-10000 and range-checks the resulting year (`DateTime.cs:1020-1032`). Exception type is
`ArgumentOutOfRangeException` in every rejecting case with .NET's paramNames — `dayNumber`,
`value`, `months`, `value` — replacing the leaked DateTime-constructor `year`. Every valid date,
day number and Add* result is byte-identical (measured, `build-probe/1837_postfix.log` case 9).
`DateOnly.hpp` is unchanged.

Public doors, all pinned by tests: `FromDayNumber`, `AddDays`, `AddMonths`, `AddYears`. The
separately documented parse-grammar and format-breadth gaps above are untouched.

## Post-audit subset remediation — #1929 row 5 (2026-08-01)

The original SR-AUD-061 and its #1879 remediation remain preserved above.
Exact approval under `docs/TextSubsetCompatibilityDecision.md` §6.5 item (3)
adds only surrounding invariant whitespace to DateOnly Parse/TryParse. Internal
whitespace, unpadded month/day, trailing garbage and timestamps remain rejected;
the pre-existing wider trailing `Z`/`z` behavior is unchanged. Parse and
TryParse agree on values and FormatException identity. This post-audit widening
does not change SR-AUD-061's remediated status or issue an identifier.
