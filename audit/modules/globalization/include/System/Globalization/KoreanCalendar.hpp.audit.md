# Audit: `modules/globalization/include/System/Globalization/KoreanCalendar.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The class implements its documented Gregorian offset and validates the mutable
two-digit year setting.  Basic era/year/date cases pass in the module target.

## Other missing assertions and diagnostics

- Test date-range ends, invalid era and year arguments, AddMonths/AddYears,
  and TwoDigitYearMax validation.

## Final assessment

No defect is confirmed in the reviewed surface.
