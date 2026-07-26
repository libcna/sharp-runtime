# Audit: `modules/core/src/System/String.cpp`

## Metadata

- Audit status: AUDITED (893 lines, full read).
- Direct probe compiled against `build/libsharp_runtime_core.a` on 2026-07-25:
  `g++ -std=c++20 -Imodules/core/include /tmp/sharp_runtimervc_string_audit_probe.cpp build/libsharp_runtime_core.a -o /tmp/sharp_runtimervc_string_audit_probe`.
- The current `StringTests` suite has no escaped-brace or substring-spill
  `LastIndexOf` assertion; the probe exposes both paths below.

## Assessment

The file has broad static utility coverage and several carefully hardened range
checks, including unsigned `ToCharArray` bounds validation.  Existing code also
contains good regressions for `Split` empty input, single-argument `Substring`,
large format indices, and backward search start-index fixups.  The bespoke
composite formatter and substring range implementation remain high-risk because
they reconstruct framework parser/range semantics manually.

## SR-AUD-015 — medium — `String::Format` rejects valid escaped braces and accepts malformed closing braces

`replaceArg` substitutes a format item before brace escaping is parsed, while
`FinalizeFormat` rejects every remaining `{` but never examines a `}`.  The
result is both directions of incorrect behavior:

- `String::Format("{{0}}", 42)` throws `FormatException` instead of returning
  the literal `{0}`;
- `String::Format("value}", 42)` returns `value}` instead of reporting malformed
  composite formatting.

The direct probe printed `escaped_exception=...` followed by `stray=value}`.
Microsoft’s composite-format specification requires `{{` and `}}` to produce
literal braces and treats malformed format strings as errors:
<https://learn.microsoft.com/en-us/dotnet/standard/base-types/composite-formatting#escaping-braces>.

### Required post-audit verification

Replace the sequential replacement parser with a single-pass parser that
distinguishes escaped braces, format items, alignment/specifier syntax, and
malformed closing braces.  Add exact tests for `{{0}}`, `{{{0:D}}}`, literal
`}}`, unmatched `{`/`}`, and out-of-range indices.  Preserve the current
multi-digit-index regression.

## SR-AUD-016 — medium — substring `LastIndexOf` can return a match that extends past the requested search end

The four-argument string overload calls `std::string::rfind(substr, startIndex)`
and validates only the match **start** against the lower range bound.  C++
`rfind` treats `startIndex` as the latest permitted start, not the final
character of a bounded .NET search range.  Consequently the probe reports:

```
last_index=3
```

for `String::LastIndexOf("abcde", "de", 3, 4)`.  The requested range is
indices 0 through 3, but the alleged match consumes index 4.  The .NET contract
defines `count` positions ending at `startIndex` as the searched section:
<https://learn.microsoft.com/en-us/dotnet/api/system.string.lastindexof?view=net-10.0>.

### Required post-audit verification

Require `matchStart + substr.length() <= startIndex + 1` as well as the lower
bound, using overflow-safe range arithmetic.  Add both a spilling negative
case (`"abcde", "de", 3, 4 -> -1`) and a same-range positive case.  Test empty
value and `startIndex == Length` behavior separately because this port has an
explicit compatibility fixup there.

## Other missing assertions and diagnostics

- `fmtInt`/`fmtDouble` use `std::stoi` for specifier precision without a
  controlled format-error translation; oversized/non-numeric precision can
  leak `std::invalid_argument`/`std::out_of_range` instead of
  `System::FormatException`.
- Replacement, trim, case, comparison, and indexing operate over UTF-8 bytes
  or C locale classification.  Their relationship to .NET UTF-16 and culture
  semantics needs explicit documented adaptation tests; ASCII-only checks are
  insufficient evidence.
- Size totals in `Concat`/`Join` have no overflow diagnostic before `reserve`;
  this is untested and relevant only near allocation limits.

## Final assessment

Useful broad helpers with two confirmed public formatting/search range defects.
The next repair phase should prioritize a parser-based formatter and bounded
substring search tests rather than adding more happy-path overload assertions.
