# Audit: `modules/core/include/System/MissingFieldException.hpp`

## Metadata

- Audit status: AUDITED (60-line inline implementation, fully read).
- Validation: the complete plural/singular member-access filter passed 61/61 on 2026-07-26.
- Reference basis: local .NET runtime exception source and `COR_E_MISSINGFIELD` (`0x80131511`).

## Assessment

Every overload first uses the `MissingMemberException` construction path then
overrides the inherited HResult with `COR_E_MISSINGFIELD`.  The class/field
overload creates the expected `Field 'Class.Field' not found.` diagnostic.
Focused tests verify that exact message and the derived HResult for all four
constructors.  No standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- Message assertions do not exercise empty, embedded quote, dotted, or UTF-8
  names, so field-label diagnostic escaping is unverified.
- The inner constructor check only looks for outer text and omits stored-inner
  identity/rethrow coverage.
- No actual runtime field-resolution path produces this exception in the
  reviewed native C++ surface; compile-time member lookup differs from .NET
  dynamic reflection semantics.

## Final assessment

Constructor delegation, exact ordinary-name formatting, inheritance, and
HResult override behavior pass the focused evidence. No source or test was
modified.
