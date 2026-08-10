// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2168 (SR-AUD-278). The finding's own defect -- 44 static members declared and never
// defined, so a consumer fails at final LINK -- cannot be expressed as a runtime assertion, and
// this file does not pretend otherwise: the compile-time half is
// test/consumer/numerics_generic_math_negative.cpp, 12 sites, and it is the discriminating
// instrument. What belongs here is the other half of the acceptance criterion: everything a
// consumer legitimately does with this header must keep working, and the one member that IS
// defined must stay defined.
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <numbers>
#include <type_traits>

#include "System/Numerics/DivisionRounding.hpp"
#include "System/Numerics/GenericMathInterfaces.hpp"

namespace N = System::Numerics;

namespace {
    // The conformance pattern this port intends: derive for the NAME, define your own statics.
    // A derived member hides the deleted base member, so this must compile and run.
    struct MyNumber : N::ISignedNumber<MyNumber>, N::IMinMaxValue<MyNumber> {
        int v = 0;
        static MyNumber MinValue() { MyNumber m; m.v = std::numeric_limits<int>::min(); return m; }
        static MyNumber MaxValue() { MyNumber m; m.v = std::numeric_limits<int>::max(); return m; }
    };
}

TEST(GenericMathInterfacesDeletionTests, TheOneDefinedMemberIsStillDefined) {
    // Radix is a `static constexpr` initialiser, not a declaration-without-definition, so it was
    // never part of SR-AUD-278 and must be untouched by its repair.
    static_assert(N::INumberBase<int>::Radix == 2);
    EXPECT_EQ(N::INumberBase<int>::Radix, 2);
    EXPECT_EQ(N::INumberBase<double>::Radix, 2);
}

TEST(GenericMathInterfacesDeletionTests, HierarchyStillInstantiates) {
    struct MyNum : N::ISignedNumber<MyNum> {};
    struct MyUNum : N::IUnsignedNumber<MyUNum> {};
    struct MyFloat : N::IFloatingPointIeee754<MyFloat> {};
    struct MyInt : N::IBinaryInteger<MyInt> {};
    MyNum n; MyUNum u; MyFloat f; MyInt i;
    (void)n; (void)u; (void)f; (void)i;
    SUCCEED();
}

TEST(GenericMathInterfacesDeletionTests, OperatorAndAggregateInterfacesStillInstantiate) {
    N::IAdditionOperators<int, int, int> a;
    N::ISubtractionOperators<int, int, int> s;
    N::IMultiplyOperators<int, int, int> m;
    N::IDivisionOperators<int, int, int> d;
    N::IModulusOperators<int, int, int> mo;
    N::IEqualityOperators<int, int, bool> e;
    N::IComparisonOperators<int, int> c;
    N::IBitwiseOperators<int, int, int> b;
    N::IShiftOperators<int, int, int> sh;
    N::IIncrementOperators<int> inc;
    N::IDecrementOperators<int> dec;
    N::IUnaryNegationOperators<int> un;
    N::IUnaryPlusOperators<int> up;
    // The aggregate that inherits seven of the deleted families still instantiates: deleting a
    // static member does not make the class unusable as a base, which is the whole point.
    N::IBinaryFloatingPointIeee754<double> ieee;
    (void)a; (void)s; (void)m; (void)d; (void)mo; (void)e; (void)c;
    (void)b; (void)sh; (void)inc; (void)dec; (void)un; (void)up; (void)ieee;
    SUCCEED();
}

TEST(GenericMathInterfacesDeletionTests, DerivedStaticsHideTheDeletedBaseMembers) {
    EXPECT_EQ(MyNumber::MinValue().v, std::numeric_limits<int>::min());
    EXPECT_EQ(MyNumber::MaxValue().v, std::numeric_limits<int>::max());
    static_assert(std::is_base_of_v<N::IMinMaxValue<MyNumber>, MyNumber>);
}

TEST(GenericMathInterfacesDeletionTests, TheDocumentedCppReplacementsProduceTheExpectedValues) {
    // The doc-comments now tell a caller what to write instead. This test is what keeps those
    // instructions honest -- if one of them is wrong, the migration advice is wrong.
    EXPECT_EQ(std::numeric_limits<int>::lowest(), std::numeric_limits<int>::min());
    EXPECT_DOUBLE_EQ(std::numbers::pi_v<double>, 3.14159265358979323846);
    EXPECT_DOUBLE_EQ(2 * std::numbers::pi_v<double>, 6.283185307179586476925286766559);
    EXPECT_DOUBLE_EQ(std::log(8.0) / std::log(2.0), 3.0);
    EXPECT_DOUBLE_EQ(std::pow(10.0, 2.0), 100.0);
    // The *Pi variants, which have no direct <cmath> spelling and are the ones a caller is most
    // likely to reach for.
    EXPECT_NEAR(std::sin(0.5 * std::numbers::pi_v<double>), 1.0, 1e-15);
    EXPECT_NEAR(std::acos(0.0) / std::numbers::pi_v<double>, 0.5, 1e-15);
    EXPECT_EQ(int{}, 0);
    EXPECT_EQ(int{1}, 1);
}

TEST(GenericMathInterfacesDeletionTests, DivisionRoundingEnumIsUnchanged) {
    EXPECT_EQ(static_cast<int>(N::DivisionRounding::Truncate), 0);
    EXPECT_EQ(static_cast<int>(N::DivisionRounding::Floor), 1);
    EXPECT_EQ(static_cast<int>(N::DivisionRounding::Ceiling), 2);
    EXPECT_EQ(static_cast<int>(N::DivisionRounding::AwayFromZero), 3);
    EXPECT_EQ(static_cast<int>(N::DivisionRounding::Euclidean), 4);
}
