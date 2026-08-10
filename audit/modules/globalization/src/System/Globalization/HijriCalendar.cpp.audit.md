# Audit: `modules/globalization/src/System/Globalization/HijriCalendar.cpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The implementation validates month/year before table access and bounds large
year offsets before multiplication.  Reviewed regression tests cover its basic
tabular conversion and invalid ranges.

## Other missing assertions and diagnostics

- Add reference-date sweeps, HijriAdjustment values, minimum/maximum tick
  round-trips, and end-of-month AddMonths behavior.

## Final assessment

No independently reproducible defect is confirmed.
