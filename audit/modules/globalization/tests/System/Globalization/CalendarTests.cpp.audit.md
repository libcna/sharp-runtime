# Audit: `modules/globalization/tests/System/Globalization/CalendarTests.cpp`

## Metadata

- Audit status: AUDITED.
- Validation: included in the 676/676 passing module target.

## Assessment

The 46 tests cover selected Korean, Japanese, Hijri, Hebrew, and Um Al Qura
values plus repaired large-argument guards.  They do not make the base
`Calendar` concrete-shape issue observable because they only use subclasses.

## Finding references

- SR-AUD-281 — medium — hierarchy still exposes a directly constructible base.

## Other missing assertions and diagnostics

- Add data-table endpoint sweeps, era-transition days, invalid input matrices,
  and time/tick round trips for every calendar.

## Final assessment

No separate test-contract defect is confirmed.
