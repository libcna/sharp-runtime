# Audit: `modules/text-regular-expressions/include/System/Text/RegularExpressions/Match.hpp`

## Metadata

- AUDITED: eager submatch ownership, Groups/Captures, continuation callback,
  Result substitution, and empty match.
- Validation: direct ASan probe creates a Match, destroys its Regex source, and
  then calls NextMatch.

## SR-AUD-245 — high — Match::NextMatch retains a raw Regex this pointer past the regex lifetime

`Match` stores a `std::function` supplied by Regex::matchFrom.  That lambda
captures raw `[this, input, nextOffset]`; after a stack Regex leaves scope,
`first.NextMatch()` calls `matchFrom` through the dead object.  ASan reports
`stack-use-after-scope` at Regex.hpp:122.  The equivalent current .NET probe
destroys its local Regex scope, forces GC, and prints `next_success=True
index=1`; the managed Match retains the state it needs for continuation.

## Assessment

Eagerly copying std::smatch values correctly avoids one iterator lifetime
hazard, but the continuation reintroduces a separate owner lifetime failure.
The limited Result token support is explicitly documented; SR-AUD-245 is not.

## Other missing assertions and diagnostics

- Add ASan Match/NextMatch-after-Regex-destruction coverage (SR-AUD-245),
  match copies/moves, empty/zero-length chains, nested groups, and evaluator
  reentrancy/exception tests.
- Add replacement token, multi-digit group, named group, unmatched group,
  Unicode index, and result/collection parity checks.

## Final assessment

SR-AUD-245 is ASan-confirmed. No source or test was changed during this audit.
