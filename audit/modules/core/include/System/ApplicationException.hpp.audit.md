# Audit: `modules/core/include/System/ApplicationException.hpp`

## Metadata

- Audit status: AUDITED (26-line inline implementation, fully read).
- Validation: the complete five-type exception-family filter passed 43/43 on 2026-07-26.
- Reference/probe: local .NET `ApplicationException.cs` assigns
  `COR_E_APPLICATION` (`0x80131600`) in every constructor; the standalone
  `/tmp/sharp-runtimervc-exception-hresult-audit-probe` prints C++ value
  `80131500`.

## SR-AUD-094 — medium — five exception types retain their base HResult instead of their documented derived diagnostic code

`ApplicationException` never calls `setHResultProperty`, so all three overloads
retain `Exception`'s `COR_E_EXCEPTION` (`0x80131500`) rather than
`COR_E_APPLICATION` (`0x80131600`). The same omission occurs in sibling
`AppDomainUnloadedException`, `BadImageFormatException`,
`CannotUnloadAppDomainException`, and `DataMisalignedException`: each retains
the `SystemException` base `COR_E_SYSTEM` (`0x80131501`) rather than its
documented type-specific code. Local .NET source assigns the correct value in
every affected overload, and the direct probe prints the inherited C++ values.
All 43 existing focused tests pass because none asserts an HResult for these
types.

## Other missing assertions and diagnostics

- Tests do not assert the exact default message, HResult for any constructor,
  stored-inner identity/rethrow behavior, or empty/UTF-8 message boundaries.
- Current .NET advises application authors not to throw/catch this legacy base
  type for ordinary errors; its retained C++ API nonetheless advertises a .NET
  counterpart and should preserve the observable diagnostic code.
- No null C-string boundary exists because the message API requires
  `std::string`.

## Final assessment

The message and inheritance paths pass, but the public HResult is incompatible.
No source or test was modified.
