# Audit: `modules/core/include/System/NotSupportedException.hpp`

## Metadata

- Audit status: AUDITED (39-line declaration, fully read).
- Validation: its shared exception tests passed within the audited 124/124
  Core.Base filter on 2026-07-26.

## Assessment

The declaration matches the expected SystemException hierarchy and constructor
surface. No standalone defect was reproduced.

## Other missing assertions and diagnostics

- Tests omit default resource text, C-string null/empty behavior, inner
  identity, all base catches, and copy/move stability.
- No stream/platform consumer proves this type is selected for unsupported
  operations instead of a generic native exception.

## Final assessment

The reviewed declaration is coherent. No source or test was modified.
