# Audit: `modules/text-regular-expressions/include/System/Text/RegularExpressions/Capture.hpp`

## Metadata

- AUDITED: capture value/index/length storage and string conversion.
- Evidence: Match/Group construction and direct Regex header probe were read.

## Assessment

The eager owned-string capture avoids retaining std::smatch iterators into a
temporary input.  It provides the essential immutable data for one capture.

## Other missing assertions and diagnostics

- No direct fixture covers unmatched/default captures, Unicode/byte indices,
  lifetime after input destruction, virtual ToString, or boundary values.

## Final assessment

No declaration-level defect was demonstrated. No source or test was changed.
