# Audit: `modules/core/src/System/SystemException.cpp`

## Metadata

- Audit status: AUDITED (32-line implementation, fully read).
- Validation: the complete `ExceptionTests.cpp` and `ExceptionNewTests.cpp`
  suite filter passed 124/124 in `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reference: local .NET `SystemException.cs` resource/default HResult path was
  reviewed.

## Assessment

All four constructor paths delegate to the native base and consistently assign
`COR_E_SYSTEM` (`0x80131501`).  The default literal `System error.` matches the
local resource wording.  No standalone implementation defect was established.

## Other missing assertions and diagnostics

- Current tests cover only a custom message and catchability; they omit the
  default literal, HResult on all constructors, inner-exception identity,
  null/empty C-string behavior, and `what()` stability after copying/moving.
- No test verifies the type via `std::exception` after a polymorphic base
  reference, or records inherited Data/StackTrace limitations.

## Final assessment

The short implementation consistently realizes the expected SystemException
message and HResult adaptation.  No source or test was modified during this
audit.
