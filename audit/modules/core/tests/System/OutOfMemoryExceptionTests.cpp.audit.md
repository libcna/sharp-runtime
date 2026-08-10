# Audit: `modules/core/tests/System/OutOfMemoryExceptionTests.cpp`

## Metadata

- AUDITED: 44-line dedicated fixture, fully read.
- Validation: `OutOfMemoryExceptionTest.*` passed 6/6 within the selected
  40-test exception filter on 2026-07-26.

## Findings

All public constructor forms are checked for `E_OUTOFMEMORY`, ordinary message
behavior, and SystemException inheritance. No standalone production defect is
reproduced.

## Missing assertions and diagnostics

- Missing exact default text, null/UTF-8 C-string, inner identity/rethrow,
  std::exception, copy/move, and allocation-failure mapping vectors.
- The fixture deliberately does not induce allocation failure, so it cannot
  establish how native `bad_alloc` is translated at project boundaries.

## Final assessment

Strong constructor/HResult smoke coverage; no source or test was modified
during this audit.
