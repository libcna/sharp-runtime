# Audit: `modules/globalization/include/System/Globalization/StringInfo.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET StringInfo](https://learn.microsoft.com/en-us/dotnet/api/system.globalization.stringinfo?view=net-10.0)
  defines text elements as graphemes, including combining-character sequences.

## Assessment

The code intentionally treats UTF-8 leading-byte sequences as elements.  Worse,
`SubstringByTextElements` validates and slices raw byte offsets, even though its
arguments are named text-element indexes.  It can return an invalid UTF-8 tail.

### SR-AUD-279 — medium — StringInfo confuses UTF-8 bytes/code points with managed text elements

The direct probe reports two elements for `e` plus U+0301 rather than one, and
`SubstringByTextElements(1, 1)` on `éa` returns only byte `A9` rather than `a`.
Enumeration, parser indexes, and element lengths share the incomplete grapheme
logic; the existing test name `ParseCombiningCharacters_SplitsEachByte` locks
the incompatible expectation.

## Finding references

- SR-AUD-279 — medium — incorrect grapheme segmentation and byte/text-element
  index confusion.

## Other missing assertions and diagnostics

- Test combining sequences, regional-indicator/emoji clusters, malformed UTF-8,
  boundary indexes, and text-element—not byte—substring ranges.

## Final assessment

SR-AUD-279 applies.
