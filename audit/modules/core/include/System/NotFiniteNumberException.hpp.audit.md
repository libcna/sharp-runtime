# Audit: `modules/core/include/System/NotFiniteNumberException.hpp`

## Metadata

- Audit status: AUDITED (103-line inline implementation, fully read).
- Validation: the focused five-type exception filter passed 29/29 on 2026-07-26.
- Reference basis: local .NET `NotFiniteNumberException.cs` and `COR_E_NOTFINITENUMBER` (`0x80131528`).

## Assessment

All six constructor forms set `COR_E_NOTFINITENUMBER` and preserve the
offending `double` where applicable. The `double`-only constructor intentionally
uses the default `ArithmeticException` base message, matching current .NET's
no-base-argument constructor. Existing tests cover ordinary infinity storage
and a finite diagnostic value. No standalone implementation defect was
confirmed.

## Other missing assertions and diagnostics

- Tests omit every HResult, the string-only/inner/three-argument constructor forms, and all special offending values (NaN payloads, positive infinity, negative infinity, and negative zero).
- No test confirms the intentionally inherited default arithmetic message for the `double`-only constructor.
- Stored-inner identity/rethrow and exact default resource text are untested.

## Final assessment

The observed constructor and HResult behavior matches the local .NET contract. No source or test was modified.
