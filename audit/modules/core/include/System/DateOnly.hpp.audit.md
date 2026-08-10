# Audit: `modules/core/include/System/DateOnly.hpp`

## Metadata

- Audit status: AUDITED (196-line public header, fully read).
- Supporting validation: `DateOnlyTests.*:TimeOnlyTests.*` passed 119/119 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Implementation evidence is in the paired
  `modules/core/src/System/DateOnly.cpp.audit.md` report; the test source also
  contains supporting TimeOnly cases and was audited as one complete file.

## Assessment

The header clearly documents the intentionally reduced, ISO-shaped C++ date
parse/format interface and exposes construction, day-number conversion,
arithmetic, calendar components, DateTime conversion, and comparisons.  The
ordinary public shape is coherent.  However, every arithmetic and day-number
operation takes a signed `intcs` public input, so the implementation must
validate or use defined unsigned/wider arithmetic before calculating an
intermediate.  The header provides no boundary diagnostic or documented
adaptation for the actual overflow paths.

## Finding references

- **SR-AUD-060:** the public `FromDayNumber`, `AddDays`, `AddMonths`, and
  `AddYears` entry points route extreme valid-`intcs` arguments into
  UBSan-confirmed signed C++ overflow before the .NET-required range result can
  be reported.
- **SR-AUD-061:** the documented ISO `TryParse` / `Parse` entry points accept
  a valid ISO prefix followed by arbitrary trailing text, rather than requiring
  the complete input to be a date.

## Other missing assertions and diagnostics

- The header's `FromDayNumber` documentation omits the required valid
  `[0, MaxDayNumber]` range and its `ArgumentOutOfRangeException` behavior.
- Public `AddDays`, `AddMonths`, and `AddYears` documentation says only that
  inputs may be negative; it does not identify result/range errors or the
  bounded .NET month/year domain.
- `ToString(format)` documents only a small token subset.  Its behavior for
  empty, unsupported standard, repeated-token, escaped-quote, or culture
  inputs is neither constrained nor diagnosed at the API boundary.

## Final assessment

The normal public type surface is understandable, but it exposes the
implementation's unsafe arithmetic and permissive parse boundary.  No source
or test was modified during this audit.
