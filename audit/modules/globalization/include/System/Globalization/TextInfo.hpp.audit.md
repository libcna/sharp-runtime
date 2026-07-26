# Audit: `modules/globalization/include/System/Globalization/TextInfo.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET casing guidance](https://learn.microsoft.com/en-us/dotnet/standard/base-types/changing-case)
  describes TextInfo casing as culture-sensitive.

## Assessment

The constructor retains a culture name but no operation consults it.  UTF-8
overloads call `std::tolower`/`toupper` independently for each byte, so in the
default C locale non-ASCII letters are unchanged.  The direct `de-DE` probe
returns UTF-8 bytes `c3a4` unchanged for `ToUpper("ä")`.

### SR-AUD-284 — medium — TextInfo advertises culture-specific Unicode casing but performs ASCII-byte casing only

`ToLower`, `ToUpper`, and `ToTitleCase` operate on byte classification and
cannot implement Unicode or culture rules.  The header also reports
right-to-left/code-page data as universal stubs.  Callers receive a successful
result indistinguishable from a real locale operation.

## Finding references

- SR-AUD-284 — medium — culture and Unicode casing behavior is silently absent.

## Other missing assertions and diagnostics

- Test Turkish I, German/Greek/non-ASCII casing, UTF-16 surrogate handling,
  RTL cultures, invalid UTF-8, and each reported code-page property.

## Final assessment

SR-AUD-284 applies.
