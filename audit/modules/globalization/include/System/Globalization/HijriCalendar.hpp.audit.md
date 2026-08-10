# Audit: `modules/globalization/include/System/Globalization/HijriCalendar.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The header and its implementation use a tabular Hijri model with explicit
argument checks.  This is a documented calendar model choice; focused tests
cover basic conversion and large AddYears argument rejection.

## Other missing assertions and diagnostics

- Test HijriAdjustment behavior, min/max dates, all eras, month end clamping,
  and current .NET data-version comparison.

## Final assessment

No independently reproducible defect is confirmed.
