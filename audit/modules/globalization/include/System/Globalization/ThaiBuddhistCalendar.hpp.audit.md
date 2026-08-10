# Audit: `modules/globalization/include/System/Globalization/ThaiBuddhistCalendar.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The fixed Gregorian-year offset and single era are simple and range guarded by
DateTime construction.  No separate conversion error was reproduced.

## Other missing assertions and diagnostics

- Test supported endpoints, era validation, leap years, AddMonths/AddYears,
  and TwoDigitYearMax validation.

## Final assessment

No defect is confirmed in the reviewed surface.
