# Audit: `modules/core/tests/System/ContextMarshalExceptionTests.cpp`

## Metadata

- AUDITED: 33-line dedicated fixture, fully read.
- Validation: `ContextMarshalExceptionTests2.*` passed 5/5 within the selected
  31-test exception filter on 2026-07-26.

## Findings

The fixture has unusually good default-text coverage and checks ordinary
message/inheritance behavior. It never reads HResult, so all passing cases
miss the confirmed SR-AUD-096 mismatch: the implementation retains
`COR_E_SYSTEM` rather than `COR_E_CONTEXTMARSHAL`.

## Missing assertions and diagnostics

- Missing HResult for all constructors, C-string/null/UTF-8, inner identity,
  std::exception, copy/move, and a real context-marshalling route.
- The inner test checks only outer text and cannot establish retained cause
  behavior.

## Final assessment

Good default message coverage, but SR-AUD-096 remains unguarded. No source or
test was modified during this audit.
