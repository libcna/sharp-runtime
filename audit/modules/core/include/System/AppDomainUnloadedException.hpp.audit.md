# Audit: `modules/core/include/System/AppDomainUnloadedException.hpp`

## Metadata

- Audit status: AUDITED (26-line inline implementation, fully read).
- Validation: the complete five-type exception-family filter passed 43/43 on 2026-07-26.
- Reference/probe: local .NET `AppDomainUnloadedException.cs` assigns
  `COR_E_APPDOMAINUNLOADED` (`0x80131014`); the shared standalone probe prints
  inherited C++ `80131501`.

## SR-AUD-094 — medium — five exception types retain their base HResult instead of their documented derived diagnostic code

This class has no `setHResultProperty` call. Its default, message, and
message-plus-inner overloads therefore retain `SystemException`'s
`COR_E_SYSTEM` (`0x80131501`) instead of .NET's
`COR_E_APPDOMAINUNLOADED` (`0x80131014`). See the owning
`ApplicationException.hpp.audit.md` finding for the shared five-class evidence.

## Other missing assertions and diagnostics

- The four shared tests only check a non-empty message, message containment,
  inheritance, and outer text; HResult, exact default text, and inner-pointer
  identity are absent.
- The implementation has no AppDomain lifecycle facility that can naturally
  throw this type. That platform adaptation is distinct from the constructor's
  incorrect diagnostic code and needs an explicit documented policy.
- No null C-string message overload exists.

## Final assessment

The constructor messages pass normal tests, but their public diagnostic code is
incorrect. No source or test was modified.
