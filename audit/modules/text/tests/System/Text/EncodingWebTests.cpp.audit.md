# Audit: `modules/text/tests/System/Text/EncodingWebTests.cpp`

## Metadata

- Audit status: AUDITED.
- Validation: included in the 233/233 focused Text run on 2026-07-27.

## Assessment

Tests exercise a few ASCII escape vectors and a happy-path URL round trip.
They label several vectors official but omit the default encoder Unicode policy,
invalid UTF-8, invalid ranges, and malformed percent diagnostics.

## Finding references

- SR-AUD-297 — medium — default Web encoder behavior and diagnostics lack
  coverage.

## Other missing assertions and diagnostics

- Add Basic-Latin/non-ASCII, control, malformed percent, truncated percent,
  and exact exception/result tests.

## Final assessment

The suite leaves SR-AUD-297 unprotected.
