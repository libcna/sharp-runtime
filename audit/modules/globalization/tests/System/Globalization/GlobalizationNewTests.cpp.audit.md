# Audit: `modules/globalization/tests/System/Globalization/GlobalizationNewTests.cpp`

## Metadata

- Audit status: AUDITED.

## Assessment

This 28-test file exercises nominal ASCII TextInfo/CompareInfo/StringInfo
cases, selected CharUnicodeInfo values, date-name validation, and calendar
overflow fixes.  It does not cover a Unicode grapheme or a culture-aware
operation.

## Finding references

- SR-AUD-279 — medium — no grapheme/text-element-index cases.
- SR-AUD-283 — medium — no Unicode or non-ASCII CompareOptions cases.
- SR-AUD-284 — medium — ASCII-only casing tests.

## Other missing assertions and diagnostics

- Add combining accents, emoji clusters, Turkish and German casing, diacritic
  comparison, and invalid CompareOptions combinations.

## Final assessment

The test gaps leave SR-AUD-279, SR-AUD-283, and SR-AUD-284 unobserved.
