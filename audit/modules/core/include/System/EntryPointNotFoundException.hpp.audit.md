# Audit: `modules/core/include/System/EntryPointNotFoundException.hpp`

## Metadata

- Audit status: AUDITED (32-line inline implementation, fully read).
- Validation: the complete type-load exception filter passed 48/48 on 2026-07-26.
- Reference/probe: local .NET `EntryPointNotFoundException.cs` assigns `COR_E_ENTRYPOINTNOTFOUND` (`0x80131523`); the shared standalone probe prints inherited C++ `80131522`.

## SR-AUD-095 — medium — DllNotFoundException and EntryPointNotFoundException retain the TypeLoad HResult

All three constructors delegate to `TypeLoadException` and never replace its
`COR_E_TYPELOAD` (`0x80131522`) with the required
`COR_E_ENTRYPOINTNOTFOUND` (`0x80131523`). The same probe demonstrates the
sibling `DllNotFoundException` omission; local .NET source assigns each
specific code. No one of the 48 passing focused tests asserts this type's
HResult. See the owning `DllNotFoundException.hpp.audit.md` report.

## Other missing assertions and diagnostics

- Tests omit TypeLoad inheritance, every HResult, exact default text, stored-inner identity/rethrow, and empty/UTF-8 message diagnostics.
- There is no native interop/entry-point-resolution path in the reviewed module that constructs the exception, so only constructor behavior is independently reproducible.

## Final assessment

Normal message paths pass but the observable diagnostic code is inherited from the wrong exception type. No source or test was modified.
