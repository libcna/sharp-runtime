# Audit: `modules/uri/include/System/UriTypeConverter.hpp`

## Metadata

- AUDITED: 73-line inline implementation, fully read.
- Validation: `UriTypeConverterTest.*` passed 7/7 within the selected 13-test
  URI converter/exception filter on 2026-07-27.
- Reference basis: local current `UriTypeConverter.cs`.

## SR-AUD-148 — medium — UriTypeConverter treats an empty string as a UriFormatException instead of the documented null conversion result

Current .NET `ConvertFrom` returns null for an empty string before constructing
a Uri. C++ always calls `Uri(text)`, which throws UriFormatException for empty
input; its non-nullable `Uri` return type cannot represent the managed null
outcome. The public comment and `ConvertFromEmptyThrows` test explicitly lock
this divergent behavior in rather than documenting an optional/sentinel
adaptation.

## Other missing assertions and diagnostics

- The minimal no-type-argument `CanConvertFrom`/`CanConvertTo` API cannot
  express .NET's source/destination type questions, Uri cloning, or
  InstanceDescriptor conversion. This declared component-model adaptation
  needs an API-baseline decision beyond the confirmed empty-input defect.
- There is no equivalent of `IsValid`, culture/context behavior, invalid
  object-type conversion, or relative Uri clone testing.
- ConvertTo uses OriginalString correctly for the supported C++ Uri path.

## Final assessment

Normal non-empty string conversion is green, but empty input exposes an
unrepresentable managed result and incorrect exception contract. No source or
test was modified during this audit.
