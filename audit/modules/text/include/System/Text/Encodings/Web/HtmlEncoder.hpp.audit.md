# Audit: `modules/text/include/System/Text/Encodings/Web/HtmlEncoder.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET character-encoder defaults](https://learn.microsoft.com/en-us/dotnet/standard/serialization/system-text-json/character-encoding)
  initializes default encoders with Basic Latin and applies encoder-specific
  block lists.

## Assessment

The implementation escapes five ASCII characters but passes non-Basic-Latin
UTF-8 bytes unchanged. The direct probe returns raw `é` for `Default`/static
encoding; .NET's default policy uses Basic Latin unless a caller creates a
relaxed encoder. Its substring overload also relies on native `substr` after
unsigned conversion rather than managed range diagnostics.

## Finding references

- SR-AUD-297 — medium — Web default encoders and diagnostics do not implement
  the declared .NET policy.

## Other missing assertions and diagnostics

- Test non-ASCII/default policy, invalid UTF-8, C0 controls, range boundaries,
  negative start/count, and the exact runtime exception type.

## Final assessment

SR-AUD-297 applies.
