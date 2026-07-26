# Audit: `modules/core/tests/System/NumberStylesExtendedTests.cpp`

## Metadata

- Audit status: AUDITED (322 lines, 43 tests, full read).
- Validation: all 43 `NumberStylesExtendedTests.*` cases passed in the focused
  167-test Core.Base run on 2026-07-25.

## Assessment

This is a high-value grammar regression suite for the shared integer parser.
It covers grouping, fractional-zero semantics and error precedence, invariant
currency marker placement, parentheses/trailing signs, interleaved whitespace,
leading-zero hexadecimal/binary bounds, binary styles, and repeated signs.  It
also deliberately samples sibling integer widths so shared parser changes do
not silently regress them.

The tests accurately document the port's invariant-only culture adaptation and
the `TryParse` false result for unsigned negative syntax.  No new parser defect
was confirmed from the reviewed paths.

## Other missing assertions and diagnostics

- Provider parameters are accepted but ignored.  Tests use only the invariant
  marker and do not prove a supplied non-invariant provider is rejected or
  visibly documented; public callers should not infer culture support.
- The extensive grammar matrix uses Int32 as its main representative.  A
  generated cross-width corpus (including min/max values) would better guard
  against future divergence in the shared parser adapters.
- No malformed UTF-8/currency-token input is supplied; the parser works on
  byte strings and should document its expected encoding at that API boundary.

## Final assessment

Focused, evidence-rich parser coverage with clearly stated adaptation bounds.
It should be retained as the shared integer parser evolves.
