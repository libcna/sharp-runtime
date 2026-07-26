# Audit: `modules/core/include/System/UnauthorizedAccessException.hpp`

## Metadata

- Audit status: AUDITED (25-line declaration, fully read with implementation).
- Validation: the direct four-fixture exception filter passed 41/41 on
  2026-07-27; its nine UnauthorizedAccessException cases are now fully
  audited in `UnauthorizedAccessExceptionTests.cpp.audit.md`.
- Reference basis: local .NET `UnauthorizedAccessException.cs` and `COR_E_UNAUTHORIZEDACCESS` (`0x80070005`).

## Assessment

The declaration exposes the expected default, C-string, string, and inner constructor set. Its implementation assigns the .NET access-denied code in each route. Existing shared tests only exercise normal messages and broad inheritance. No standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- The direct fixture checks the default HResult, but it omits C-string null
  behavior, exact default text, stored-inner identity/rethrow, UTF-8 input,
  and HResult preservation for non-default constructors.
- No reviewed filesystem/OS access error mapping proves that denied native operations are consistently translated into this type.

## Final assessment

Declaration and normal implementation behavior are consistent. No source or test was modified.
