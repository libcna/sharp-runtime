# Audit: `modules/uri/tests/System/UriPartialTests.cpp`

## Metadata

- AUDITED: 28-line dedicated fixture, fully read.
- Validation: `UriPartialTest.*` passed 5/5 within the selected 38-test URI
  value-type filter on 2026-07-27.

## Findings

The fixture checks only the four enum values, leaving SR-AUD-150's absent
GetLeftPart API completely unobserved.

## Missing assertions and diagnostics

- Missing every advertised Uri.GetLeftPart vector and invalid enum handling.

## Final assessment

Correct constants do not establish a usable UriPartial contract. No source or
test was modified.
