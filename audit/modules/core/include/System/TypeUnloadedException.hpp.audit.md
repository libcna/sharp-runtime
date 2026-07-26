# Audit: `modules/core/include/System/TypeUnloadedException.hpp`

## Metadata

- Audit status: AUDITED (32-line inline implementation, fully read).
- Validation: the complete type-load exception filter passed 48/48 on 2026-07-26.
- Reference basis: local .NET `TypeUnloadedException.cs` and `COR_E_TYPEUNLOADED` (`0x80131013`).

## Assessment

All four overloads explicitly assign `COR_E_TYPEUNLOADED`, including the C++
`const char*` convenience overload. The direct test verifies the default code,
ordinary message, inheritance, and throw/catch behavior. `SystemException`'s
underlying C-string path safely turns null into empty text rather than reading
through it. No standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- HResult is checked only for the default constructor; no test covers the C-string null case, other overloads, stored-inner identity/rethrow, or empty/UTF-8 text.
- This module supplies no unloadable runtime type system, so callers cannot observe a natural throw path beyond manual construction.

## Final assessment

The inline diagnostic-code and constructor behavior are consistent with the implemented contract. No source or test was modified.
