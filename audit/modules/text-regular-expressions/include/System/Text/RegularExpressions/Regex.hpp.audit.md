# Audit: `modules/text-regular-expressions/include/System/Text/RegularExpressions/Regex.hpp`

## Metadata

- AUDITED: std::regex compilation/options, named-group stripping, Match/Matches,
  Replace/Split/Escape, and continuation construction.
- Validation: direct C++ ASan and current-.NET lifetime probes; public headers
  compiled directly because this INTERFACE module has no standalone configured
  fixture target.

## SR-AUD-245 — high — Match::NextMatch retains a raw Regex this pointer past the regex lifetime

`matchFrom` returns Match with a continuation `[this, input, nextOffset]`.
Destroying a stack Regex then calling NextMatch on its prior Match produces
ASan `stack-use-after-scope` while `std::regex_search` reads the dead `re_` at
line 122.  Current .NET continues to the next match after local scope and
forced GC.  See Match's report for exact probe structure.

## Assessment

The header explicitly documents grammar/options/timeout reductions and has
carefully repaired index/anchor/named-group behavior for supported patterns.
The raw continuation pointer is nevertheless a reachable memory-safety bug.

## Other missing assertions and diagnostics

- Add SR-AUD-245 ASan regression, no-fixture header compilation, and standard
  Match/Matches/Replace/Split/Escape coverage.
- Test invalid patterns/error offsets, all RegexOptions/unknown flags, named
  syntax inside escaped/classes/lookarounds, zero-length paths, evaluator
  exceptions, backtracking resource behavior, timeout limitation, Unicode, and
  replacement token parity.

## Final assessment

SR-AUD-245 is ASan-confirmed. No source or test was changed during this audit.
