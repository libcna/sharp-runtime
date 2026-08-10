# Audit: `modules/uri/include/System/UriPartial.hpp`

## Metadata

- AUDITED: 20-line enum declaration, fully read.
- Validation: `UriPartialTest.*` passed 5/5 within the selected 38-test URI
  value-type filter on 2026-07-27.

## SR-AUD-150 — medium — UriPartial has no Uri.GetLeftPart consumer in the public C++ API

The enum documentation names `System::Uri::GetLeftPart`, but the audited C++
Uri declaration/implementation has no such method. Current .NET exposes
`Uri.GetLeftPart(UriPartial)` for all four values. The valid enum is therefore
unusable for its advertised operation and has no first-party consumer.

## Other missing assertions and diagnostics

- Tests cover only integer values, not Scheme/Authority/Path/Query string
  extraction, invalid enum values, opaque URIs, or relative URI behavior.

## Final assessment

The values are correct but the promised public operation is absent. No source
or test was modified during this audit.
