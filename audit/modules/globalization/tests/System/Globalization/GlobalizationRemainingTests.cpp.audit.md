# Audit: `modules/globalization/tests/System/Globalization/GlobalizationRemainingTests.cpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The file primarily asserts constants and simple value holders.  Its
`StringInfoTests::ParseCombiningCharacters_SplitsEachByte` expectation directly
preserves code-point/byte segmentation rather than .NET text elements.

## Finding references

- SR-AUD-279 — medium — named assertion locks the incompatible segmentation.

## Other missing assertions and diagnostics

- Replace byte-splitting expectations with grapheme vectors and add invalid
  enum-value consumers rather than constant-only tests.

## Final assessment

This test source exposes SR-AUD-279.
