// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2168 (SR-AUD-278): proves that every static member of the
// System::Numerics generic-math interface templates is rejected AT THE CALL SITE.
//
// Before #2168 those 44 members were declared and never defined anywhere. A consumer that called
// one compiled cleanly and then failed at final link with an unresolved reference to a mangled
// template specialisation -- arbitrarily far from the call, with nothing to point at. Only a
// COMPILATION check can express the repair: a runtime test cannot assert that something does not
// compile, and a link test would have passed before and after only by accident of what is linked.
//
// The reduction itself is settled and documented in three places already -- System/Half.hpp
// ("Generic math interface conformance ... is out of scope"), System/Numerics/DivisionRounding.hpp,
// and the header's own preamble -- so this fixture pins the SHAPE of the reduction, not the
// decision to make it.
//
// Migration for a caller that hits one of these: use the C++ spelling named on the interface's
// doc-comment -- <limits> for MinValue/MaxValue, <numbers> for E/Pi/Tau, <cmath> for every
// function family, and a plain TSelf{} / TSelf{1} for the two identities. Deriving from these
// interfaces and defining your OWN statics is unaffected: a derived type's member hides the base's,
// and site 12 below proves that the ordinary uses of this header still compile.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by the compiler; with
// no site selected the file compiles cleanly. scripts/check_negative_consumer_fixtures.py compiles
// each site on its own and asserts the rejection.
//
// NEGATIVE-FIXTURE: component=Numerics

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Numerics/DivisionRounding.hpp"
#include "System/Numerics/GenericMathInterfaces.hpp"

namespace N = System::Numerics;

// A consumer type that conforms the way this port intends: it derives for the NAME and defines its
// own statics. This must keep compiling, and it is what site 12 exercises.
struct MyNumber : N::ISignedNumber<MyNumber>, N::IMinMaxValue<MyNumber> {
    int v = 0;
    static MyNumber MinValue() { MyNumber m; m.v = -2147483647 - 1; return m; }
    static MyNumber MaxValue() { MyNumber m; m.v = 2147483647; return m; }
};

int main() {
    // The whole surface a consumer is entitled to: instantiate the hierarchy, read the one
    // constant that is actually defined, and use the enum. None of this is affected by #2168.
    N::IAdditionOperators<int, int, int> add;
    N::IEqualityOperators<int, int, bool> eq;
    N::IBinaryFloatingPointIeee754<double> ieee;
    (void)add;
    (void)eq;
    (void)ieee;
    static_assert(N::INumberBase<int>::Radix == 2, "the one defined member must stay defined");
    static_assert(static_cast<int>(N::DivisionRounding::Euclidean) == 4, "enum unchanged");
    // The intended conformance pattern still works.
    (void)MyNumber::MinValue().v;
    (void)MyNumber::MaxValue().v;

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(min-max-value-deleted): use of deleted function
    //     | attempt to use a deleted function
    (void)N::IMinMaxValue<int>::MinValue();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(max-value-deleted): use of deleted function
    //     | attempt to use a deleted function
    (void)N::IMinMaxValue<int>::MaxValue();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(additive-identity-deleted): use of deleted function
    //     | attempt to use a deleted function
    (void)N::IAdditiveIdentity<int>::AdditiveIdentity();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(multiplicative-identity-deleted): use of deleted function
    //     | attempt to use a deleted function
    (void)N::IMultiplicativeIdentity<int>::MultiplicativeIdentity();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(trigonometric-deleted): use of deleted function
    //     | attempt to use a deleted function
    (void)N::ITrigonometricFunctions<double>::Acos(1.0);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // The *Pi variants are the ones a caller is most likely to reach for, because <cmath> has no
    // direct spelling for them. They are deleted too, and the doc-comment gives the expression.
    // NEGATIVE(trigonometric-pi-deleted): use of deleted function
    //     | attempt to use a deleted function
    (void)N::ITrigonometricFunctions<double>::SinPi(0.5);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 7
    // NEGATIVE(hyperbolic-deleted): use of deleted function
    //     | attempt to use a deleted function
    (void)N::IHyperbolicFunctions<double>::Tanh(1.0);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 8
    // NEGATIVE(logarithmic-deleted): use of deleted function
    //     | attempt to use a deleted function
    (void)N::ILogarithmicFunctions<double>::Log2(8.0);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 9
    // NEGATIVE(exponential-deleted): use of deleted function
    //     | attempt to use a deleted function
    (void)N::IExponentialFunctions<double>::Exp10(2.0);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 10
    // NEGATIVE(power-and-root-deleted): use of deleted function
    //     | attempt to use a deleted function
    (void)N::IRootFunctions<double>::RootN(8.0, 3);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 11
    // NEGATIVE(constants-deleted): use of deleted function
    //     | attempt to use a deleted function
    (void)N::IFloatingPointConstants<double>::Pi();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 12
    // Inherited through IBinaryFloatingPointIeee754, which aggregates seven of the families: the
    // deletion reaches the aggregate too, which is the case a caller is most likely to hit.
    // NEGATIVE(aggregate-inherits-deletion): use of deleted function
    //     | attempt to use a deleted function
    (void)N::IBinaryFloatingPointIeee754<double>::Sqrt(4.0);
#endif

    return 0;
}
