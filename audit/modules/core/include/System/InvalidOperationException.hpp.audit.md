# Audit: `modules/core/include/System/InvalidOperationException.hpp`

## Metadata

- Audit status: AUDITED (35-line declaration, fully read).
- Validation: its dedicated supplemental fixture passed 5/5 within the
  audited 58-test member/type-access exception filter on 2026-07-27.

## Assessment

The SystemException inheritance and three public C++ constructor forms match
the intended native adaptation; their distinct `COR_E_INVALIDOPERATION`
(`0x80131509`) HResult is directly asserted. No standalone defect was
reproduced.

## Other missing assertions and diagnostics

- Dedicated tests cover non-empty/default, custom C-string, outer-inner text,
  and every available constructor's HResult. They still omit exact default
  text, null/UTF-8 C-string behavior, inner identity/rethrow, and
  std::exception polymorphism.
- No stateful consumer verifies that an invalid operation is raised at the
  correct transition rather than as a generic native failure.

## Final assessment

The declaration is consistent with its implementation. No source or test was modified.
