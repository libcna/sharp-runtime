# Audit: `modules/core/include/System/DataMisalignedException.hpp`

## Metadata

- Audit status: AUDITED (29-line inline implementation, fully read).
- Validation: the complete five-type exception-family filter passed 43/43 on 2026-07-26.
- Reference/probe: local .NET `DataMisalignedException.cs` assigns
  `COR_E_DATAMISALIGNED` (`0x80131541`); the shared standalone probe prints
  inherited C++ `80131501`.

## SR-AUD-094 — medium — five exception types retain their base HResult instead of their documented derived diagnostic code

The three inline constructors never call `setHResultProperty`, leaving
`COR_E_SYSTEM` (`0x80131501`) in place of
`COR_E_DATAMISALIGNED` (`0x80131541`). Normal message and inheritance tests
pass, but none asserts HResult. The local .NET source and shared probe provide
the directly reproducible mismatch; see the owning
`ApplicationException.hpp.audit.md` report for the family scope.

## Other missing assertions and diagnostics

- Tests omit HResult for all overloads, exact default text, retained-inner
  identity/rethrow, and non-ASCII message behavior.
- No audited native unaligned load/store API maps its platform behavior to this
  exception, so an actual hardware/alignment integration diagnostic remains
  untested.
- No null C-string message overload exists.

## Final assessment

Message and inheritance behavior pass ordinary tests, but the visible
type-specific HResult is missing. No source or test was modified.
