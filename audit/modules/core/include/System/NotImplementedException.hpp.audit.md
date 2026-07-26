# Audit: `modules/core/include/System/NotImplementedException.hpp`

## Metadata

- Audit status: AUDITED (33-line declaration, fully read).
- Validation: its shared exception tests passed within the audited 124/124
  Core.Base filter on 2026-07-26.

## Assessment

The declaration correctly exposes SystemException inheritance and constructor
forms, including the documented native C-string convenience overload.

## Other missing assertions and diagnostics

- Tests omit exact default text, C-string null/empty input, inner identity,
  copy/move, and catchability through std::exception.
- No API-boundary test distinguishes intentional unsupported behavior (which
  should use this type) from unfinished implementation or native errors.

## Final assessment

No standalone defect was reproduced. No source or test was modified.
