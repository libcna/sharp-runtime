# Audit: `modules/core/include/System/UnauthorizedAccessException.hpp`

## Metadata

- Audit status: AUDITED (25-line declaration, fully read with implementation).
- Validation: the focused exception-family filter passed 38/38 on 2026-07-26.
- Reference basis: local .NET `UnauthorizedAccessException.cs` and `COR_E_UNAUTHORIZEDACCESS` (`0x80070005`).

## Assessment

The declaration exposes the expected default, C-string, string, and inner constructor set. Its implementation assigns the .NET access-denied code in each route. Existing shared tests only exercise normal messages and broad inheritance. No standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- Tests omit every HResult, C-string null behavior, exact default text, stored-inner identity/rethrow, and UTF-8 input.
- No reviewed filesystem/OS access error mapping proves that denied native operations are consistently translated into this type.

## Final assessment

Declaration and normal implementation behavior are consistent. No source or test was modified.
