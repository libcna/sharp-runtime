# Audit: `modules/core/include/System/ContextMarshalException.hpp`

## Metadata

- Audit status: AUDITED (58-line inline implementation, fully read).
- Validation: the focused protection/context/program exception filter passed 32/32 on 2026-07-26.
- Reference/probe: local .NET `Context.cs` assigns `COR_E_CONTEXTMARSHAL` (`0x80131504`); the shared standalone probe prints inherited C++ `80131501`.

## SR-AUD-096 — medium — AccessViolationException and ContextMarshalException omit their distinct HResults

None of the default, C-string, string, or message-plus-inner constructors calls
`setHResultProperty`. They therefore leave `SystemException`'s
`COR_E_SYSTEM` (`0x80131501`) instead of
`COR_E_CONTEXTMARSHAL` (`0x80131504`). The C# implementation's terminal
constructor assigns the required code; the C++ probe and 32 green tests show
that no local test asserts it. See the owning AccessViolation report for the
shared two-class finding.

## Other missing assertions and diagnostics

- Existing tests correctly preserve the ordinary default message but omit every HResult, null C-string behavior, stored-inner identity/rethrow, and UTF-8 diagnostics.
- The module has no remoting/context-marshalling facility that can construct this type naturally; that platform boundary needs an explicit policy and an integration diagnostic.

## Final assessment

Message and inheritance paths pass, but all public constructors expose the wrong HResult. No source or test was modified.
