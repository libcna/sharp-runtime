# Audit: `modules/uri/tests/System/UriTypeConverterTests.cpp`

## Metadata

- AUDITED: 57-line dedicated fixture, fully read.
- Validation: `UriTypeConverterTest.*` passed 7/7 within the selected 13-test
  URI converter/exception filter on 2026-07-27.

## Findings

`ConvertFromEmptyThrows` requires UriFormatException, the opposite of current
.NET's null empty-string conversion. It therefore locks SR-AUD-148 in place.
The remaining cases exercise only ordinary string-to-Uri and Uri-to-string
paths.

## Missing assertions and diagnostics

- Missing an explicitly documented optional/sentinel result for empty input.
- Missing type-directed CanConvertFrom/CanConvertTo behavior, Uri clone,
  InstanceDescriptor, IsValid, culture/context, and invalid object-type paths.
- The OriginalString test uses an absolute URI only; a relative URI is needed
  to exercise the stated managed round-trip reason.

## Final assessment

Useful normal conversion smoke coverage, but one green test asserts a confirmed
managed-contract mismatch. No source or test was modified.
