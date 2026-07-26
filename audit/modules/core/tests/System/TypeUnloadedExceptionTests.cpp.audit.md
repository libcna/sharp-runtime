# Audit: `modules/core/tests/System/TypeUnloadedExceptionTests.cpp`

## Metadata

- AUDITED: 49-line dedicated fixture, fully read.
- Validation: `TypeUnloadedExceptionTest.*` passed 8/8 within the selected
  40-test exception filter on 2026-07-26.

## Findings

The fixture checks nonempty/unload-oriented ordinary text, hierarchy, standard
catchability, throwability, and default `COR_E_TYPEUNLOADED`. No standalone
production defect is reproduced.

## Missing assertions and diagnostics

- Missing HResult for custom/inner constructors, exact default text,
  C-string/null/UTF-8, inner identity/rethrow, and copy/move vectors.
- No loader/AppDomain-style consumer can trigger an actual unload diagnostic,
  so the type is tested only as a manually constructed exception.

## Final assessment

Good constructor/HResult smoke coverage; no source or test was modified during
this audit.
