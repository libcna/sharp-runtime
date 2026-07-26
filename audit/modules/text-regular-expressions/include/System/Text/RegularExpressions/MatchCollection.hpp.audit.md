# Audit: `modules/text-regular-expressions/include/System/Text/RegularExpressions/MatchCollection.hpp`

## Metadata

- AUDITED: owned Match vector, count/index validation, and iterators.
- Evidence: Regex::Matches construction and Match ownership model were read.

## Assessment

The collection eagerly owns the Match values returned by std::sregex_iterator,
so basic match data survives source input destruction.  NextMatch continuation
is not installed by Matches and the separate raw Regex lifetime issue is
limited to Match()/Match_ chains (SR-AUD-245).

## Other missing assertions and diagnostics

- Add empty/out-of-range, zero-length, named/multiline/Unicode match,
  iterator, copied collection, and post-Regex-lifetime tests.

## Final assessment

No independent defect was demonstrated. No source or test was changed.
