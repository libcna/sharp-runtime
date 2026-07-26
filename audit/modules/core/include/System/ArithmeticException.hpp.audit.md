# Audit: `modules/core/include/System/ArithmeticException.hpp`

## Metadata

- Audit status: AUDITED (70-line inline public implementation, fully read).
- Validation: `ArithmeticExceptionTests.*` passed 4/4 in the focused
  Core.Base exception filter on 2026-07-26; the complete shared exception
  fixture was audited separately and passed 124/124.
- Reference: local .NET `ArithmeticException.cs` HResult/default resource path
  was reviewed.

## Assessment

The inline constructors consistently use the expected arithmetic default text,
inherit from SystemException, retain the inner exception, and assign
`COR_E_ARITHMETIC` (`0x80070216`).  The C-string overload is an explicit C++
convenience adaptation and delegates null handling safely to Exception.

## Other missing assertions and diagnostics

- Direct tests do not check HResult for default/message/C-string/inner
  constructors, C-string null/empty input, exact resource text, or inner
  exception identity.
- No test covers copy/move, catchability as `std::exception`, non-ASCII/
  embedded-NUL messages, or the inherited Data/Source/HelpLink adaptation.
- The header has no diagnostic distinguishing C++ native arithmetic UB from a
  deliberate `ArithmeticException`; callers can still receive signals or
  undefined behavior before constructing this class.

## Final assessment

The reviewed constructor contract is consistent with current .NET for the
implemented native adaptation.  No source or test was modified during this
audit.
