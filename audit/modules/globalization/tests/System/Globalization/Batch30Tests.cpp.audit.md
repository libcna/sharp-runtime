# Audit: `modules/globalization/tests/System/Globalization/Batch30Tests.cpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The batch covers representative Hijri/ISOWeek/Japanese/IDN paths.  Its Idn
property test only checks storage and never checks whether AllowUnassigned
changes a mapping operation.

## Finding references

- SR-AUD-282 — medium — setting coverage does not test behavioral effect.

## Other missing assertions and diagnostics

- Add U+0378/assignment policy tests in both modes, full ISO year boundaries,
  Japanese transitions, and Hijri endpoint vectors.

## Final assessment

The test gap leaves SR-AUD-282 unobserved.
