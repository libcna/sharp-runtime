# Audit: `modules/text/include/System/Text/Encodings/Web/UrlEncoder.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET UrlEncoder.Create](https://learn.microsoft.com/en-us/dotnet/api/system.text.encodings.web.urlencoder.create?view=net-10.0)
  exposes an encoding policy rather than this additional decoder API.

## Assessment

Encoding performs RFC-3986-style percent conversion, but the extra public
`Decode` method calls `std::stoi` for malformed percent text. The direct probe
of `%zz` surfaces native `std::invalid_argument` (`stoi`) rather than a
documented runtime result or diagnostic.

## Finding references

- SR-AUD-297 — medium — Web default encoders and diagnostics do not implement
  the declared .NET policy.

## Other missing assertions and diagnostics

- Test malformed/truncated percent sequences, non-ASCII UTF-8, plus handling,
  policy customization, and exact exception/result semantics.

## Final assessment

SR-AUD-297 applies.
