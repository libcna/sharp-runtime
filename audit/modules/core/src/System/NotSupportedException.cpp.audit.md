# Audit: `modules/core/src/System/NotSupportedException.cpp`

## Metadata

- Audit status: AUDITED (33-line implementation, fully read).
- Validation: shared tests passed within the audited 124/124 Core.Base filter.

## Assessment

All constructors assign `COR_E_NOTSUPPORTED` (`0x80131515`) and retain the
local .NET default message. No defect was established.

## Other missing assertions and diagnostics

- Direct coverage verifies selected HResults only; it omits C-string/null,
  exact text, and inner exception identity.
- The duplicated literal assignment has no centralized taxonomy assertion.

## Final assessment

The implementation is correct for reviewed inputs. No source or test was modified.
