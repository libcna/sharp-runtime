# Audit: `modules/globalization/include/System/Globalization/CompareInfo.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET CompareOptions](https://learn.microsoft.com/en-us/dotnet/api/system.globalization.compareoptions?view=net-10.0)
  defines culture-aware IgnoreNonSpace, IgnoreSymbols, IgnoreKanaType, and
  IgnoreWidth behavior and restricts ordinal option combinations.

## Assessment

The implementation lowers ASCII bytes only.  It neither uses its culture name
nor implements five published option bits, and it accepts invalid option
combinations.  The direct probe returns nonzero for `Ä` versus `ä` with
`IgnoreCase`; nonspacing and symbol options are likewise simply ignored.

### SR-AUD-283 — medium — CompareInfo silently reduces declared Unicode/culture comparison options to ASCII byte comparison

`Compare`, searches, sort keys, and hashes have the same incomplete option
handling, so callers can receive mutually inconsistent localization results
without an unsupported-option diagnostic.  Current .NET explicitly gives
IgnoreNonSpace and IgnoreSymbols semantic effect and rejects prohibited ordinal
combinations.

## Finding references

- SR-AUD-283 — medium — public comparison semantics and option validation are incomplete.

## Other missing assertions and diagnostics

- Add `café`/`cafe` IgnoreNonSpace, punctuation IgnoreSymbols, kana/width,
  Unicode case, and invalid ordinal-combination tests.
- Assert Compare/IndexOf/SortKey/hash consistency for each supported option.

## Final assessment

SR-AUD-283 applies.
