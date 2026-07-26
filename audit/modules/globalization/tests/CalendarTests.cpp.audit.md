# Audit: `modules/globalization/tests/CalendarTests.cpp`

## Metadata

- Audit status: AUDITED.
- Validation: included in the 676/676 passing Globalization test target.

## Assessment

The 82 tests exercise Gregorian-like fallback behavior, including many direct
`Calendar cal;` constructions.  That makes the test green while asserting a
public shape that current .NET deliberately forbids.

## Finding references

- SR-AUD-281 — medium — direct base Calendar construction is test-locked.

## Other missing assertions and diagnostics

- Replace base-instance behavior tests with a concrete test calendar and add
  an API-shape compile check for abstractness.

## Final assessment

This test source exposes SR-AUD-281.
