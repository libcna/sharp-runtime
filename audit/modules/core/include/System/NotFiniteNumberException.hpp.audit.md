# Audit: `modules/core/include/System/NotFiniteNumberException.hpp`

## Metadata

- Audit status: AUDITED (103-line inline implementation, fully read).
- Validation: `NotFiniteNumberExceptionTest.*` passed 9/9 on 2026-07-27; its
  complete direct fixture audit is in `NotFiniteNumberExceptionTests.cpp.audit.md`.
- Reference basis: local .NET `NotFiniteNumberException.cs` and `COR_E_NOTFINITENUMBER` (`0x80131528`).

## Assessment

All six constructor forms set `COR_E_NOTFINITENUMBER` and preserve the
offending `double` where applicable. The `double`-only constructor intentionally
uses the default `ArithmeticException` base message, matching current .NET's
no-base-argument constructor. Direct tests cover all public construction
routes, infinity/NaN storage, the number-only base-message quirk, and every
constructor's HResult. An independent C++/managed probe confirms the six-route
HResult matrix. No standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- Tests omit NaN payload/sign, negative zero, stored-inner identity/rethrow,
  null/empty/non-ASCII messages, and exact default resource text.
- No floating-point producer is tested end to end, so construction coverage
  does not prove what real failure path selects this exception.

## Final assessment

The observed constructor and HResult behavior matches the local .NET contract. No source or test was modified.
