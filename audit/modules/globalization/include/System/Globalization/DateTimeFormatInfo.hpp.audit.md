# Audit: `modules/globalization/include/System/Globalization/DateTimeFormatInfo.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The invariant data, writable checks, range guards, and copy-returning name
arrays were reviewed.  The header explicitly omits the managed Calendar
property and always reports invariant current information; this is a documented
large locale-data adaptation rather than an unreported local crash path.

## Other missing assertions and diagnostics

- Test every array setter for invalid empty/duplicate naming data and all era
  values.
- Add integration tests showing whether DateTime formatting actually observes
  mutable pattern values and culture-specific calendar state.

## Final assessment

No new separately classified defect is confirmed.
