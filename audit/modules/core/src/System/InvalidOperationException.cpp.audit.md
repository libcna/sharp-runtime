# Audit: `modules/core/src/System/InvalidOperationException.cpp`

## Metadata

- Audit status: AUDITED (31-line implementation, fully read).
- Validation: shared tests passed within the audited 124/124 Core.Base filter.

## Assessment

All constructor paths set `COR_E_INVALIDOPERATION` (`0x80131509`) and the
default resource text matches local .NET. No defect was established.

## Other missing assertions and diagnostics

- No direct HResult/default-text/null-C-string/inner-identity test exists.
- Repeated literal HResult assignment has no shared compile-time taxonomy check.

## Final assessment

Construction behavior is correct for reviewed inputs. No source or test was modified.
