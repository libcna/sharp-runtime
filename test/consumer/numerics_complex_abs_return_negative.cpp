// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2172 (SR-AUD-277 remainder).
//
// #2172 changed System::Numerics::Complex::Abs's return type from Complex to
// double, matching .NET (`Complex.cs:292`), and removed the invented `AbsD`
// that had grown beside it precisely because the return type was wrong.
//
// There is no implicit conversion in either direction -- Complex(double, double)
// has no defaulted second parameter -- so an affected caller gets a HARD
// COMPILE ERROR rather than a silent change. That is the whole reason this
// fixture can exist: both spellings computed the same magnitude, so no value
// comparison could have caught the difference.
//
// Migration: assign the result to a double, and replace AbsD with Abs.
//
// Records: docs/Migration-ComplexAbsReturnsDouble.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Numerics allow=int128-extension
#include <type_traits>

#include "System/Numerics/Complex.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Numerics::Complex;

int main() {
    const Complex z(3.0, 4.0);

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(complex-abs-assigned-to-complex): conversion from
    //     | cannot convert
    //     | no viable conversion
    // The local is deliberately scoped and unused: enabling this site changes its TYPE, and a
    // later line that consumed it would report a second, unrelated diagnostic outside the site
    // region -- which the checker rejects, and rightly, because the site's own verdict could not
    // then be attributed to its own source.
    { Complex magnitude = Complex::Abs(z); (void)magnitude; }
#else
    { const double magnitude = Complex::Abs(z); (void)magnitude; }
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(complex-absd-removed): is not a member of
    //     | has not been declared
    //     | no member named
    const double viaAbsD = Complex::AbsD(z);
    (void)viaAbsD;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(complex-abs-still-returns-complex): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY: a trait or decltype on the return type.
    static_assert(std::is_same_v<decltype(Complex::Abs(z)), Complex>,
                  "Abs is expected to return a Complex");
#else
    static_assert(std::is_same_v<decltype(Complex::Abs(z)), double>,
                  "#2172: Abs returns a double, as .NET's does");
#endif

    // UNCHANGED, and asserted so the fixture proves the change was surgical: the other complex
    // -> complex factories keep their types. Abs was the odd one out, not the pattern.
    static_assert(std::is_same_v<decltype(Complex::Sqrt(z)), Complex>, "Sqrt still returns Complex");
    static_assert(std::is_same_v<decltype(Complex::Exp(z)), Complex>, "Exp still returns Complex");
    const double checked = Complex::Abs(z);
    return checked > 4.9 && checked < 5.1 ? 0 : 1;
}
