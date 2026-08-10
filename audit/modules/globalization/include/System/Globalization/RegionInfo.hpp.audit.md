# Audit: `modules/globalization/include/System/Globalization/RegionInfo.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

Only US data is real; any nonempty caller-provided identifier becomes invented
name/ISO/currency metadata.  Together with CultureInfo's acceptance of unknown
names, this silently converts an invalid locale request into plausible data.

### SR-AUD-285 — medium — CultureInfo and RegionInfo accept unknown identifiers and fabricate locale metadata

`RegionInfo("not-a-region")` is accepted and returns truncated pseudo-ISO
values plus USD defaults, while CultureInfo accepts an arbitrary name and
derives properties from it.  Current .NET resolves names through culture data
and throws its culture/argument exception for unsupported identifiers.

## Finding references

- SR-AUD-285 — medium — unsupported locale identifiers do not fail and give
  callers fabricated data.

## Other missing assertions and diagnostics

- Add known/unknown neutral/specific culture and region name/LCID vectors,
  case-normalization checks, and explicit behavior for no-locale-data builds.

## Final assessment

SR-AUD-285 applies.
