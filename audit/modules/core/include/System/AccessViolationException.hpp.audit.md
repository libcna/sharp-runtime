# Audit: `modules/core/include/System/AccessViolationException.hpp`

## Metadata

- Audit status: AUDITED (26-line inline implementation, fully read).
- Validation: the focused protection/context/program exception filter passed 32/32 on 2026-07-26.
- Reference/probe: local .NET `AccessViolationException.cs` sets `E_POINTER` (`0x80004003`) for all constructors; `/tmp/sharp-runtimervc-exception-hresult-audit-probe` prints C++ `80131501`.

## SR-AUD-096 — medium — AccessViolationException and ContextMarshalException omit their distinct HResults

This header delegates every constructor to `SystemException` but never calls
`setHResultProperty`, so the public value remains `COR_E_SYSTEM` (`0x80131501`)
rather than .NET's `E_POINTER` (`0x80004003`).
`ContextMarshalException` has the same omission and retains that base code
instead of `COR_E_CONTEXTMARSHAL` (`0x80131504`). Local .NET sources assign
their derived value in every overload, while the shared C++ probe prints the
inherited value for both types. The 32 passing focused tests omit their HResult.

## Other missing assertions and diagnostics

- Tests cover ordinary messages and base inheritance, but omit every HResult, exact default text, stored-inner identity/rethrow, and empty/UTF-8 messages.
- No reviewed managed/native memory-protection boundary turns a native fault into this exception. It remains constructible as a diagnostic type, but the missing integration does not justify the wrong code.

## Final assessment

The normal message path passes, but the observable HResult is incompatible. No source or test was modified.
