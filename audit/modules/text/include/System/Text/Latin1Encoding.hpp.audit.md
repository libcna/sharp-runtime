# Audit: `modules/text/include/System/Text/Latin1Encoding.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET Encoding](https://learn.microsoft.com/en-us/dotnet/api/system.text.encoding?view=windowsdesktop-9.0)
  identifies `Latin1` as ISO-8859-1.

## Assessment

The class maps the runtime's UTF-8 storage bytes directly rather than decoding
to Unicode scalars and re-encoding Latin-1. The probe shows U+00E9 encodes as
`c3a9` instead of `e9`, while byte `e9` decodes to an invalid one-byte UTF-8
tail instead of UTF-8 `c3a9`. Its raw GetString also lacks argument validation.

## Finding references

- SR-AUD-286 — high — signed raw decode ranges are not validated.
- SR-AUD-289 — medium — Latin-1 conversion treats UTF-8 storage bytes as
  Latin-1 characters.

## Other missing assertions and diagnostics

- Test every byte 0x80–0xFF, replacement policy, invalid signed ranges,
  embedded NUL, and Unicode round trips.

## Final assessment

SR-AUD-286 and SR-AUD-289 apply.
