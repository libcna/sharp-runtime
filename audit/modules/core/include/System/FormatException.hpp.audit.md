# Audit: `modules/core/include/System/FormatException.hpp`

## Metadata

- Audit status: AUDITED (27-line declaration, fully read with implementation).
- Validation: the focused exception-family filter passed 38/38 on 2026-07-26.
- Reference basis: local .NET `FormatException.cs` and `COR_E_FORMAT` (`0x80131537`).

## Assessment

The public declaration exposes default, C-string, string, and inner
constructors implemented with the expected HResult. Direct tests verify default
text presence, ordinary custom/inner messages, inheritance, and default code.
No standalone constructor defect was confirmed.

## Other missing assertions and diagnostics

- Tests omit HResults for non-default overloads, C-string null behavior, exact default text, stored-inner identity/rethrow, and UTF-8 messages.
- Many audited parse/format routines already throw this type; their error-type coverage does not assert the nested diagnostic state supplied by this declaration.

## Final assessment

Declaration and implementation agree on the normal exception contract. No source or test was modified.
