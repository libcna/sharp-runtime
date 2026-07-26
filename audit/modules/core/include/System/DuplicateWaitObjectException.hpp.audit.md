# Audit: `modules/core/include/System/DuplicateWaitObjectException.hpp`

## Metadata

- Audit status: AUDITED (45-line inline implementation, fully read).
- Validation: the focused exception-family filter passed 38/38 on 2026-07-26.
- Reference/probe: local .NET `DuplicateWaitObjectException.cs` assigns `COR_E_DUPLICATEWAITOBJECT` (`0x80131529`); the shared HResult probe prints C++ `80070057`.

## SR-AUD-100 — medium — DuplicateWaitObjectException inherits ArgumentException HResult and uses a divergent default diagnostic

None of the four constructors sets `COR_E_DUPLICATEWAITOBJECT`, so instances
retain `ArgumentException`'s `COR_E_ARGUMENT` (`0x80070057`) instead of
`0x80131529`. The default C++ text, `Duplicate objects in argument.`, also does
not match the current .NET diagnostic identifying duplicate objects in the wait
array. Local .NET source assigns the derived HResult in every overload and the
probe reproduces the inherited C++ code. The three current tests check only
non-empty/custom text and broad inheritance.

## Other missing assertions and diagnostics

- Tests omit every overload HResult, the default exact diagnostic, parameter-name suffix format, stored-inner identity/rethrow, and empty/UTF-8 parameter/message paths.
- No reviewed wait-handle/multi-wait implementation constructs this type for duplicate input, so the exception is only manually exercised.

## Final assessment

Constructor diagnostic compatibility is incomplete. No source or test was modified.
