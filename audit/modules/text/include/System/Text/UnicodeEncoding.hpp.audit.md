# Audit: `modules/text/include/System/Text/UnicodeEncoding.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET Encoding.Preamble](https://learn.microsoft.com/en-us/dotnet/api/system.text.encoding.preamble?view=net-10.0)
  treats the preamble as an optional prefix separate from encoding output.

## Assessment

UTF-16 scalar and surrogate conversion is otherwise careful. Raw decode
arguments are unchecked; configured BOM bytes are emitted by `GetBytes`; the
encoder/decoder fallback properties are not consulted; and an odd trailing
byte is discarded.

## Finding references

- SR-AUD-286 — high — signed raw decode ranges are not validated.
- SR-AUD-291 — medium — BOM configuration is incorrectly emitted by
  `GetBytes` rather than exposed as a preamble.
- SR-AUD-292 — medium — configured fallback policies are ignored.
- SR-AUD-293 — medium — incomplete fixed-width input is silently dropped.

## Other missing assertions and diagnostics

- Test BOM preamble separately, odd byte counts, fallback exception paths,
  null/negative ranges, and all endian configurations.

## Final assessment

SR-AUD-286 and SR-AUD-291 through SR-AUD-293 apply.
