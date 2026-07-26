# Audit: `modules/core/include/System/InvalidOperationException.hpp`

## Metadata

- Audit status: AUDITED (35-line declaration, fully read).
- Validation: its shared exception tests passed within the audited 124/124
  Core.Base filter on 2026-07-26.

## Assessment

The SystemException inheritance and four constructor forms match the intended
native adaptation; no standalone defect was reproduced.

## Other missing assertions and diagnostics

- Shared tests cover one custom message and catch only; they omit default text,
  `COR_E_INVALIDOPERATION`, all constructor forms, null C-string, inner
  identity, and std::exception polymorphism.
- No stateful consumer verifies that an invalid operation is raised at the
  correct transition rather than as a generic native failure.

## Final assessment

The declaration is consistent with its implementation. No source or test was modified.
