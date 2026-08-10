# Audit: `modules/core/tests/DateTimePropertiesTests.cpp`

## Metadata

- AUDITED: 32 DateTime construction, component, calendar, arithmetic, and
  formatting cases.
- Validation: the complete Core.Base fixture passed 4,946/4,946.

## Assessment

The fixture exercises normal Gregorian dates, invalid month/day, leap years,
component access, weekday/day-of-year, `Today`, formatting, and simple
arithmetic.  It does not exercise invalid hour/minute/second/millisecond
components or malformed parser input, so it cannot detect existing SR-AUD-006
or SR-AUD-007.

## Other missing assertions and diagnostics

- Add both signs and exact boundaries for all four time components, including
  `9999-12-31 24:00` tick-invariant protection.
- Add false/throw parser pairs for trailing junk and partial times, asserting
  that failed `TryParse` leaves the output argument unchanged.
- Cover Min/Max boundary arithmetic, precision beyond milliseconds, and
  deterministic time-provider injection rather than a broad `Today` year
  sanity check alone.

## Final assessment

The fixture is useful normal calendar coverage but misses SR-AUD-006/007
regressions. No source or test was changed.
