# Audit: `modules/time-zone/include/System/TimeZone.hpp`

## Metadata

- AUDITED: 61-line legacy abstract `TimeZone` compatibility surface and its
  `CurrentTimeZone` entry point.
- Validation: `TimeZoneTest.*` passed 8/8 on 2026-07-27.  A direct native
  comparison ran under `TZ=America/New_York` for January and July 2024.
- Reference basis: current .NET 10 obsolete-but-supported `System.TimeZone`
  CurrentTimeZone/GetUtcOffset/IsDaylightSavingTime behavior.

## SR-AUD-223 — medium — legacy CurrentTimeZone freezes one current offset and reports no daylight saving time for every date

The local adapter snapshots `TimeZoneInfo::Local().BaseUtcOffset` during
static construction; `GetUtcOffset` always returns that snapshot and
`IsDaylightSavingTime` always returns false.  With `TZ=America/New_York`, the
native probe prints winter/summer offsets `-4,-4` and DST `0,0`, whereas the
matching .NET 10 probe prints `-5,-4` and `False,True`.  The behavior corrupts
historical and future legacy-zone conversions whenever local DST rules apply.

## Assessment

The abstract custom-subclass surface is coherent.  Its default local adapter
is not an adequate compatibility path for a date-sensitive time-zone API.

## Other missing assertions and diagnostics

- Current tests inspect only the current name/reference, never winter versus
  summer dates under a deterministic DST `TZ` setting; add that process-isolated
  regression for SR-AUD-223.
- Test fixed-offset zones, local timezone reload/cache behavior, invalid and
  ambiguous local times, and exception propagation from the underlying
  TimeZoneInfo provider.

## Final assessment

SR-AUD-223 is confirmed by direct C++/current-.NET comparison.  No production
or test source was changed.
