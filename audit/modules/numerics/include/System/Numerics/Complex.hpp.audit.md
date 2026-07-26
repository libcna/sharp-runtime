# Audit: `modules/numerics/include/System/Numerics/Complex.hpp`

## Metadata

- Audit status: AUDITED (partial `std::complex<double>` adaptation).
- Validation: `ComplexTests` passed 49/49 in the 2026-07-27 Numerics run.
- Direct probe: `/tmp/sharp-runtime-numerics-audit/complex_format.cpp` prints
  `<1.000000; 2.000000>` for `(1,2)`.

## Assessment

Arithmetic, polar conversion, reciprocal-zero handling, and the tested
transcendental subset delegate to defined standard-library operations. The
public `Abs` signature returns a `Complex` with zero imaginary part instead of
.NET's `double`; a differently named `AbsD` carries the compatible result. The
default string also fixes six fractional digits and uses angle brackets and a
semicolon, while current .NET formatting emits a culture/formatted parenthesized
pair (for example `(26.1, 18.06)` in the official constructor example). Both
observable divergences are SR-AUD-277.

### SR-AUD-277 — medium — Complex Abs signature and default text diverge from .NET

`Complex::Abs` is declared as `Complex`, whereas the .NET source declares
`double Abs(Complex)`. The direct C++ probe also establishes the incompatible
fixed-six-decimal text; `AbsD` cannot preserve source compatibility because it
has a different public name.

References: [current .NET Complex source](https://raw.githubusercontent.com/dotnet/runtime/main/src/libraries/System.Runtime.Numerics/src/System/Numerics/Complex.cs)
and [official constructor example](https://learn.microsoft.com/en-us/dotnet/api/system.numerics.complex.-ctor?view=net-10.0).

## Finding references

- SR-AUD-277 — medium — `Complex::Abs` has the wrong public return type and
  `ToString` does not implement the corresponding default .NET representation.

## Other missing assertions and diagnostics

- Add a compile-time return-type assertion for `Abs`, exact default and
  formatted/culture-sensitive text checks, special values, and overflowed
  division vectors.
- State the intended scope for omitted overloads, generic-math interfaces,
  numeric conversions, parsing, and span formatting.

## Final assessment

SR-AUD-277 applies. No implementation was changed during this audit.
