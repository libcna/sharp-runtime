# Audit: `modules/core/include/System/SystemException.hpp`

## Metadata

- Audit status: AUDITED (35-line public declaration, fully read).
- Validation: the complete `ExceptionTests.cpp` and `ExceptionNewTests.cpp`
  suite filter passed 124/124 in `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reference: local .NET `SystemException.cs` was compared.

## Assessment

The small declaration correctly derives from `System::Exception` and exposes
the standard default/message/inner constructor family.  It has no new
independently demonstrated defect; its public behavior inherits the base
message, Data, stack-trace, and inner-exception adaptation boundaries.

## Finding references

- **SR-AUD-092 (context):** the inherited base default-message behavior is
  incompatible for a default `Exception`; `SystemException` supplies its own
  nonempty default resource string and is not the origin of that finding.

## Other missing assertions and diagnostics

- No direct fixture validates the default message exactness, `COR_E_SYSTEM`
  HResult on every constructor, or retained inner exception identity.
- Tests do not cover null C-string message input, empty/Unicode messages,
  copy/move behavior, or catches through both `System::Exception` and
  `std::exception` after an inner exception exists.
- The declaration does not repeat inherited permanent limitations, so a user
  reading only this common base lacks a link to stack trace/Data/reflection
  adaptation documentation.

## Final assessment

The inheritance/constructor surface is consistent with local .NET for the
implemented native adaptation.  No source or test was modified during this
audit.
