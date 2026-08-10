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

---

## Post-audit correction and partial remediation — tickets #2171 / #2172 / #2174 (2026-08-10)

*Appended by review #2167. The original report above is retained verbatim.*

**The text half is worse than recorded, and its target is contested by this report's own two
citations.** Measured (`build-probe/2167_probe1_numerics_before.log`), `std::to_string`'s fixed six
fractional digits meant `Complex(1e-9, 0)` printed `<0.000000; 0.000000>` — the value destroyed,
not merely coarsened — and `Complex(1e300, -1e300)` printed a **619-character** string of two
309-digit expansions. Infinity and NaN came out as the C library's `inf`/`-inf`/`nan`, not .NET's
`Infinity`/`-Infinity`/`NaN`. This was the only site in the port rendering a `double` with
`std::to_string`.

**Premise corrected.** The report calls the port's `<a; b>` skeleton a divergence and cites .NET's
**constructor documentation example** (`(26.1, 18.06)`) as the target; the same report also links
the **current .NET `Complex` source**, which gives `<a; b>`. Two citations in one report disagree,
`/rv` is absent, and no managed probe measured this member — so the bracket and separator
characters are **deliberately unchanged and pinned**, owned by **#2174**.

**Repair (ticket #2171, done).** Both components render through `System::Double::ToString`, this
port's own settled renderer for a .NET `double`: shortest round-trippable form and the .NET special
-value spellings. The target is a decision this repository already made, which is what makes this
half decidable with `/rv` absent, and the change is strictly closer to .NET under **either**
reading of the bracket question. `ComplexTests.ToStringFormat` asserted `s.find("1.") != npos` —
it **pinned the defect** — and was replaced, not deleted.

**Signature half (ticket #2172, needs_user).** `decltype(Complex::Abs(z))` is `Complex`,
`decltype(Complex::AbsD(z))` is `double`. Changing `Abs`'s return type is source-breaking with **no
implicit conversion either way** (`Complex(double, double)` has no defaulted second parameter), so
an affected caller gets a hard compile error rather than a silent change. **Zero in-repository
callers use `Complex::Abs`**; all three call sites use `AbsD`. A public return-type change is the
class gated as approval **D-B** (#2030) in `docs/ConsolidatedApprovalPackage.md`.

**Status:** `confirmed` → **`confirmed (design-complete)`**.
See `docs/SystemNumericsNamespaceReviewPlan.md` §4.5, §4.6, §6.3, §12.
