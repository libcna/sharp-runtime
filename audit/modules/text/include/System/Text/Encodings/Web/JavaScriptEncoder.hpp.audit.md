# Audit: `modules/text/include/System/Text/Encodings/Web/JavaScriptEncoder.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET character-encoder defaults](https://learn.microsoft.com/en-us/dotnet/standard/serialization/system-text-json/character-encoding).

## Assessment

The class converts a narrow byte subset but passes all non-ASCII UTF-8 bytes
unchanged. The direct probe returns raw `é`, despite the default Basic-Latin
allow-list contract and the local class claiming to encode JavaScript text.

## Finding references

- SR-AUD-297 — medium — Web default encoders and diagnostics do not implement
  the declared .NET policy.

## Other missing assertions and diagnostics

- Test non-ASCII scalars, UTF-8 validity, default blocked characters, line
  separators, C0/C1 controls, and encoder options.

## Final assessment

SR-AUD-297 applies.
