# Audit: `modules/core/tests/System/DataMisalignedExceptionTests.cpp`

## Metadata

- AUDITED: 28-line dedicated fixture, fully read.
- Validation: `DataMisalignedExceptionTests2.*` passed 4/4 within the selected
  31-test exception filter on 2026-07-26.

## Findings

Default/custom/inner text and SystemException catchability pass, but no test
observes the type-specific `COR_E_DATAMISALIGNED` value. This leaves the
already confirmed SR-AUD-094 inherited-base-HResult defect unguarded.

## Missing assertions and diagnostics

- Missing HResult, exact default text, C-string/null/UTF-8, inner identity,
  std::exception, copy/move, and a misaligned-memory consumer route.
- No test distinguishes a managed DataMisaligned diagnostic from a native
  alignment fault or undefined behavior.

## Final assessment

Normal constructor smoke coverage only; SR-AUD-094 remains unguarded. No
source or test was modified during this audit.
