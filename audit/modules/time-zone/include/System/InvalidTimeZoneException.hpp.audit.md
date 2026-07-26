# Audit: `modules/time-zone/include/System/InvalidTimeZoneException.hpp`

## Metadata

- AUDITED: 61-line InvalidTimeZoneException declaration and all public
  constructor routes.
- Validation: direct C++/current-.NET 10 probe compared the default HResult
  on 2026-07-27; both print `0x80131500`.

## Assessment

The exception derives from the local Exception base and preserves default,
message, and native inner-exception construction.  Its default HResult agrees
with current .NET.  No direct test source exists for this header, so ordinary
construction was checked only by the direct probe.

## Other missing assertions and diagnostics

- Add focused tests for all constructors, message/null input, HResult, inner
  exception identity, throw/catch hierarchy, and real invalid-zone producer
  translation.
- Current local TimeZoneInfo loading does not expose a controlled corrupt-zone
  seam, so `InvalidTimeZoneException` production delivery is untested.

## Final assessment

No new finding.  No production or test source was changed.
