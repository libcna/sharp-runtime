# Audit: `modules/text-regular-expressions/include/System/Text/RegularExpressions/GroupCollection.hpp`

## Metadata

- AUDITED: owned group vector/name map, indexed and named lookups, and
  iterator surface.
- Evidence: Match::Groups construction was read.

## Assessment

Numeric lookup validates bounds and absent named lookup returns the local empty
Group adaptation.  The collection owns all values rather than borrowing regex
or input state.

## Other missing assertions and diagnostics

- Add empty/out-of-range lookup, unknown/numeric/duplicate names, iteration,
  copy/move, and managed named-group parity coverage.

## Final assessment

No independent defect was demonstrated. No source or test was changed.
