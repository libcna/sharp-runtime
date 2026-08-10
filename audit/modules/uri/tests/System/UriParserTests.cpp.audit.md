# Audit: `modules/uri/tests/System/UriParserTests.cpp`

## Metadata

- AUDITED: 86-line dedicated fixture, fully read.
- Validation: `UriParserTest.*` passed 14/14 on 2026-07-27.

## Findings

The fixture validates the ordinary built-in lookup table and case-insensitivity,
but leaves SR-AUD-146 and SR-AUD-147 unguarded. It has no custom-registration
route and treats public `GetComponents` throwing NotImplementedException as an
expected baseline, despite current .NET's protected functional hook contract.

## Missing assertions and diagnostics

- Missing empty/malformed scheme rejection, which the current .NET functional
  suite requires to throw ArgumentOutOfRangeException.
- Missing Register availability, null/duplicate parser and scheme validation,
  default-port range checks, and custom-scheme construction/component vectors.
- Missing a derived parser that overrides the protected hooks and observes URI
  construction or component resolution.

## Final assessment

Useful builtin-name smoke coverage, but it cannot detect either confirmed
UriParser compatibility defect. No source or test was modified.
