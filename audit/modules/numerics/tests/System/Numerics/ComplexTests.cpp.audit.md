# Audit: `modules/numerics/tests/System/Numerics/ComplexTests.cpp`

## Metadata

- Audit status: AUDITED (49 direct tests, all passed).

## Assessment

The tests provide broad ordinary arithmetic and transcendental smoke coverage,
but `ToStringFormat` only searches for `1.` and `2.`. It therefore explicitly
accepts the local fixed-six-decimal angle-bracket form rather than checking the
actual default .NET representation. It also calls `AbsD`, masking the wrong
public `Complex::Abs` return type. These omissions leave SR-AUD-277 green.

## Finding references

- SR-AUD-277 — medium — Complex public Abs/type and default formatting parity
  are not asserted.

## Other missing assertions and diagnostics

- Add compile-time `Abs` return-type checking, exact default/format/provider
  output, NaN/infinity/signed-zero equality behavior, and extreme division
  vectors.

## Final assessment

SR-AUD-277 applies through the unobserved public contract.
