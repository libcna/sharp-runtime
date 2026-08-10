# Audit: `modules/core/tests/System/NotFiniteNumberExceptionTests.cpp`

## Metadata

- AUDITED: 78-line direct exception fixture, fully read.
- Validation: `NotFiniteNumberExceptionTest.*` passed 9/9 on 2026-07-27.
- Reference/probe: audited `NotFiniteNumberException.hpp` and local current-.NET
  `NotFiniteNumberException.cs`; C++20 and managed probes each printed
  `80131528` for default, number-only, message, message/number,
  message/inner, and full constructor HResults.

## Assessment

The fixture covers all six exposed construction routes, positive/negative
infinity and NaN storage, the managed number-only default-message quirk, and
the exception-specific HResult for every route.  The independent C++/managed
probe confirms the HResult matrix rather than relying on the source comment.
Those observed contracts agree with current .NET.  No new implementation
defect is demonstrated.

## Other missing assertions and diagnostics

- The NaN case uses only `std::isnan`; it does not preserve/check a payload or
  sign, and positive infinity is not asserted in the full constructor path.
- Inner-construction cases omit stored-inner identity/rethrow, causal message
  behavior, and both inner HResult/exception type distinctions.
- `IsArithmeticException` is only a reference-bind compile smoke test.  No
  case demonstrates a real floating-point producer choosing this exception
  instead of an overflow/format error.
- Null/empty/non-ASCII messages and exact default resource messages are not
  covered (aside from the specifically documented number-only base message).

## Final assessment

The direct fixture materially improves constructor/HResult regression coverage
and matches the managed behavior it documents.  No new finding and no source
or test change.
