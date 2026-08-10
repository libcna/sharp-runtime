# Audit: `modules/core/include/System/InvalidProgramException.hpp`

## Metadata

- Audit status: AUDITED (57-line inline implementation, fully read).
- Validation: the focused protection/context/program exception filter passed 32/32 on 2026-07-26.
- Reference basis: local .NET `InvalidProgramException.cs` and `COR_E_INVALIDPROGRAM` (`0x8013153A`).

## Assessment

The sealed class assigns `COR_E_INVALIDPROGRAM` after each base construction.
Focused tests cover all three HResults, ordinary message behavior, and
inheritance. No standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- Exact default resource text, stored-inner pointer identity/rethrow, and empty/UTF-8 message paths are not asserted.
- No reviewed IL/metadata verification or native program-validation surface can produce this exception, so it remains a manually constructed diagnostic type.

## Final assessment

The examined public constructor and HResult contract is internally consistent. No source or test was modified.
