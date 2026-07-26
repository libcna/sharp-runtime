# Audit: `modules/core/include/System/RankException.hpp`

## Metadata

- Audit status: AUDITED (29-line inline implementation, fully read).
- Validation: the focused five-type exception filter passed 29/29 on 2026-07-26.
- Reference basis: local .NET `RankException.cs` and `COR_E_RANK` (`0x80131517`).

## Assessment

The default, message, and inner overloads consistently set `COR_E_RANK` after
`SystemException` construction. Existing fixtures pass their normal message and
base-inheritance paths. No standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- Tests omit every HResult, exact default text, stored-inner identity/rethrow, and empty/UTF-8 messages.
- The array surface reviewed so far does not expose rank-sensitive multidimensional operations that naturally produce this exception; that integration contract remains untested.

## Final assessment

The inline constructor code has the expected diagnostic assignment. No source or test was modified.
