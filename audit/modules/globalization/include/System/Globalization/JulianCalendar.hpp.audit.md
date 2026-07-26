# Audit: `modules/globalization/include/System/Globalization/JulianCalendar.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

Julian leap-year and Gregorian conversion logic is isolated in the header and
covered by the module regression target.  It inherits the hierarchy concern in
SR-AUD-281 but no separate conversion defect was reproduced.

## Finding references

- SR-AUD-281 — medium — Calendar base public shape differs from .NET.

## Other missing assertions and diagnostics

- Add min/max supported dates, invalid eras, leap-century conversions, and
  AddMonths/AddYears clamping tests.

## Final assessment

No additional defect is confirmed.
