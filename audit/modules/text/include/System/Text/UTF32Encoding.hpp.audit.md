# Audit: `modules/text/include/System/Text/UTF32Encoding.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET UTF32Encoding.GetBytes](https://learn.microsoft.com/en-us/dotnet/api/system.text.utf32encoding.getbytes?view=net-10.0)
  states that `GetBytes` does not prepend a preamble.

## Assessment

UTF-32 scalar conversion validates normal units, but raw decode arguments are
unchecked. `GetBytes` serializes the configured BOM as payload, configured
fallbacks are ignored, and one-to-three trailing bytes disappear without the
decoder fallback path.

## Finding references

- SR-AUD-286 — high — signed raw decode ranges are not validated.
- SR-AUD-291 — medium — BOM configuration is incorrectly emitted by
  `GetBytes` rather than exposed as a preamble.
- SR-AUD-292 — medium — configured fallback policies are ignored.
- SR-AUD-293 — medium — incomplete fixed-width input is silently dropped.

## Other missing assertions and diagnostics

- Test byte-order preamble separately from payload, all truncated lengths,
  exception/replacement fallback, raw signed ranges, and static UTF32 factory.

## Final assessment

SR-AUD-286 and SR-AUD-291 through SR-AUD-293 apply.
