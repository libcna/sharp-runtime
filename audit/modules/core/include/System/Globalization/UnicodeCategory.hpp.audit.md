# Audit: `modules/core/include/System/Globalization/UnicodeCategory.hpp`

## Metadata

- AUDITED: 46-line public enum declaration, fully read.
- Validation: `CharTests2.*` passed 63/63 on 2026-07-27; C++/managed Unicode
  probes compared representative category values.
- Reference basis: local current-.NET `UnicodeCategory.cs` and
  `CharUnicodeInfo.cs`.

## Assessment

All thirty managed `UnicodeCategory` names retain their ordinal values 0
through 29.  The enum is therefore a correct public value vocabulary.  It
does not itself supply Unicode data or classification; the incorrect producer
behavior is owned by `CharUnicodeInfo.hpp` (SR-AUD-174).

## Other missing assertions and diagnostics

- The direct fixture checks only UppercaseLetter, LowercaseLetter, and
  DecimalDigitNumber.  It omits every other enum value, raw ordinal stability,
  and categorization of punctuation, marks, separators, symbols, private-use,
  surrogate, and unassigned code points.
- No compile-time assertion guards all thirty ordinal values even though this
  enum is a data-interchange surface for Char predicates.

## Final assessment

The enum declaration matches the managed ordinal table.  No new finding and
no source or test change.
