# Audit: `modules/text/include/System/Text/Rune.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET Rune](https://learn.microsoft.com/en-us/dotnet/api/system.text.rune?view=net-10.0)
  exposes Unicode category and case predicates.

## Assessment

UTF-8 decoding and scalar validation are carefully implemented, but public
letter/digit/case methods deliberately operate on ASCII only. The direct probe
reports `Rune(U+00E9).IsLetter() == false` and leaves it unchanged under
`ToUpper`, unlike the Unicode-category contract.

## Finding references

- SR-AUD-294 — medium — Rune Unicode classification and casing are reduced to
  ASCII despite their public names and .NET counterpart.

## Other missing assertions and diagnostics

- Test Latin, Greek, non-BMP letters, Unicode digits, titlecase, case mappings,
  invalid sequences, and unchanged output on failed decoding.

## Final assessment

SR-AUD-294 applies.
