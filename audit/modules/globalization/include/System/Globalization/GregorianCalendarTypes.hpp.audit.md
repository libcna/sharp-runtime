# Audit: `modules/globalization/include/System/Globalization/GregorianCalendarTypes.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The published values are consumed and range-checked by `GregorianCalendar`.

## Other missing assertions and diagnostics

- Exercise every value and rejection of arbitrary casts through construction
  and the mutable property.

## Final assessment

No standalone defect is confirmed.
