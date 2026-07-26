# Audit: `modules/core/include/System/DllNotFoundException.hpp`

## Metadata

- Audit status: AUDITED (25-line inline implementation, fully read).
- Validation: the complete type-load exception filter passed 48/48 on 2026-07-26.
- Reference/probe: local .NET `DllNotFoundException.cs` assigns `COR_E_DLLNOTFOUND` (`0x80131524`) in all constructors; the shared `/tmp/sharp-runtimervc-exception-hresult-audit-probe` prints C++ `80131522`.

## SR-AUD-095 — medium — DllNotFoundException and EntryPointNotFoundException retain the TypeLoad HResult

Neither constructor set in this header calls `setHResultProperty`, so all
instances retain `TypeLoadException`'s `COR_E_TYPELOAD` (`0x80131522`) instead
of `COR_E_DLLNOTFOUND` (`0x80131524`). `EntryPointNotFoundException` repeats
the same omission and inherited value rather than its documented
`COR_E_ENTRYPOINTNOTFOUND` (`0x80131523`). Current .NET source assigns each
derived code in every overload; the shared probe prints `80131522` for both
C++ types. The 48 passing family tests never assert either derived HResult.

## Other missing assertions and diagnostics

- Existing shared tests only check non-empty/default or supplied message and broad `Exception` inheritance. They omit the TypeLoad base relationship, every HResult, inner-pointer behavior, exact diagnostic text, and UTF-8 message boundaries.
- No reviewed P/Invoke or native-library resolver maps a missing dynamic library into this type. That missing integration is a separate adaptation boundary, not a reason to retain the wrong constructor code.

## Final assessment

The public diagnostic HResult is wrong despite normal message tests passing. No source or test was modified.
