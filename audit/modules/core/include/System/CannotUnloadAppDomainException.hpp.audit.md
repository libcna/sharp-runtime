# Audit: `modules/core/include/System/CannotUnloadAppDomainException.hpp`

## Metadata

- Audit status: AUDITED (25-line inline implementation, fully read).
- Validation: the complete five-type exception-family filter passed 43/43 on 2026-07-26.
- Reference/probe: local .NET `CannotUnloadAppDomainException.cs` assigns
  `COR_E_CANNOTUNLOADAPPDOMAIN` (`0x80131015`); the shared standalone probe
  prints inherited C++ `80131501`.

## SR-AUD-094 — medium — five exception types retain their base HResult instead of their documented derived diagnostic code

All three overloads delegate to `SystemException` but never replace its
`COR_E_SYSTEM` (`0x80131501`) value with .NET's
`COR_E_CANNOTUNLOADAPPDOMAIN` (`0x80131015`). Existing tests check normal
messages and inheritance but omit HResult entirely; the shared probe confirms
the inherited value. See the owning `ApplicationException.hpp.audit.md` report
for the complete five-class finding.

## Other missing assertions and diagnostics

- Tests omit exact default text, every HResult, stored-inner identity/rethrow,
  and empty/UTF-8 message boundaries.
- The module has no unloadable AppDomain implementation that can produce this
  exception. That lifecycle gap is a separate platform-adaptation decision and
  must not conceal the observable constructor-code mismatch.
- No null C-string message API exists.

## Final assessment

The basic constructor behavior passes, while the required public HResult does
not. No source or test was modified.
