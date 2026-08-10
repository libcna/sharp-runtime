# Audit: `modules/text-regular-expressions/include/System/Text/RegularExpressions/Group.hpp`

## Metadata

- AUDITED: Group inheritance, success/name state, and capture collection
  synthesis.
- Evidence: Match::Groups construction and named-group parser were read.

## Assessment

The group owns value/index/length/name data and creates a coherent single
capture for a participating group.  Quantified intermediate capture history is
explicitly outside the backing engine's visibility.

## Other missing assertions and diagnostics

- Add unnamed/named/unmatched/duplicate-name groups, nested captures,
  quantified capture limitation, group index/name parity, and lifetime tests.

## Final assessment

No declaration-level defect was demonstrated. No source or test was changed.
