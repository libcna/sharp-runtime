# Audit: `modules/core/tests/System/FormatExceptionTests.cpp`

## Metadata

- AUDITED: 33-line dedicated fixture, fully read.
- Validation: `FormatExceptionTests.*` passed 5/5 in the combined 36-test
  exception filter on 2026-07-26.
- Related production audit: `FormatException.hpp.audit.md` confirms the
  declared/implemented normal constructor contract.

## Findings

The default HResult (`COR_E_FORMAT`) is explicitly asserted, alongside ordinary
message and inheritance paths. No standalone constructor defect is reproduced.

## Missing assertions and diagnostics

- HResult is checked only for the default constructor, not custom or inner
  constructor paths.
- Missing C-string constructor/null behavior, exact resource text, inner
  exception identity/rethrow, UTF-8/embedded-NUL, copy/move, and std::exception
  vectors.
- No parser/formatter consumer asserts that its malformed input reaches this
  type with the expected retained cause and message context.

## Final assessment

The fixture protects the primary default diagnostic but leaves constructor and
consumer boundaries shallow. No source or test was modified during this audit.
