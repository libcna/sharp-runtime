# Audit: `modules/uri/include/System/UriComponents.hpp`

## Metadata

- AUDITED: 41-line flags declaration, fully read.
- Validation: `UriComponentsTest.*` passed 12/12 within the selected 38-test
  URI value-type filter on 2026-07-27.

## Assessment

All named values and composite masks match the local .NET reference, including
the unsigned bit pattern for `SerializationInfoString`. The provided `|` and
`&` operators support the direct fixture's flag combinations. No standalone
value defect was reproduced.

## Missing assertions and diagnostics

- Tests omit UserInfo, Port, StrongPort, NormalizedHost, KeepDelimiter,
  SerializationInfoString, and StrongAuthority exact values.
- The audited Uri surface has no public GetComponents operation consuming these
  flags; SR-AUD-146 records custom parser participation separately.

## Final assessment

Representable values and composites are correct; consumer behavior remains
outside this enum-only header. No source or test was modified.
