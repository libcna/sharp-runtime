// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ValueTuple.hpp"
#include <type_traits>
#include <string>
#include "System/Tuple.hpp"
#include "System/TupleExtensions.hpp"

using System::Tuple2;
using System::Tuple3;
using System::Tuple4;

// ---------------------------------------------------------------------------
// Tuple2
// ---------------------------------------------------------------------------

TEST(TupleTests, Tuple2_ConstructAndAccessItems) {
    Tuple2<int, std::string> t(42, "hello");
    EXPECT_EQ(t.getItem1Property(), 42);
    EXPECT_EQ(t.getItem2Property(), "hello");
}

TEST(TupleTests, Tuple2_IntInt_Items) {
    Tuple2<int, int> t(10, 20);
    EXPECT_EQ(t.getItem1Property(), 10);
    EXPECT_EQ(t.getItem2Property(), 20);
}

TEST(TupleTests, Tuple2_EqualityTrue) {
    Tuple2<int, int> a(1, 2);
    Tuple2<int, int> b(1, 2);
    EXPECT_TRUE(a == b);
}

TEST(TupleTests, Tuple2_EqualityFalse_DifferentItem1) {
    Tuple2<int, int> a(1, 2);
    Tuple2<int, int> b(9, 2);
    EXPECT_FALSE(a == b);
}

TEST(TupleTests, Tuple2_EqualityFalse_DifferentItem2) {
    Tuple2<int, int> a(1, 2);
    Tuple2<int, int> b(1, 9);
    EXPECT_FALSE(a == b);
}

TEST(TupleTests, Tuple2_InequalityTrue) {
    Tuple2<int, int> a(1, 2);
    Tuple2<int, int> b(3, 4);
    EXPECT_TRUE(a != b);
}

TEST(TupleTests, Tuple2_InequalityFalse_SameValues) {
    Tuple2<int, int> a(5, 6);
    Tuple2<int, int> b(5, 6);
    EXPECT_FALSE(a != b);
}

TEST(TupleTests, Tuple2_StringString) {
    Tuple2<std::string, std::string> t("key", "value");
    EXPECT_EQ(t.getItem1Property(), "key");
    EXPECT_EQ(t.getItem2Property(), "value");
}

TEST(TupleTests, Tuple2_DoubleDouble_Equality) {
    Tuple2<double, double> a(1.5, 2.5);
    Tuple2<double, double> b(1.5, 2.5);
    EXPECT_TRUE(a == b);
}

TEST(TupleTests, Tuple2_BoolInt) {
    Tuple2<bool, int> t(true, 99);
    EXPECT_TRUE(t.getItem1Property());
    EXPECT_EQ(t.getItem2Property(), 99);
}

// ---------------------------------------------------------------------------
// Tuple3
// ---------------------------------------------------------------------------

TEST(TupleTests, Tuple3_ConstructAndAccessItems) {
    Tuple3<int, std::string, double> t(1, "abc", 3.14);
    EXPECT_EQ(t.getItem1Property(), 1);
    EXPECT_EQ(t.getItem2Property(), "abc");
    EXPECT_DOUBLE_EQ(t.getItem3Property(), 3.14);
}

TEST(TupleTests, Tuple3_IntIntInt_Items) {
    Tuple3<int, int, int> t(10, 20, 30);
    EXPECT_EQ(t.getItem1Property(), 10);
    EXPECT_EQ(t.getItem2Property(), 20);
    EXPECT_EQ(t.getItem3Property(), 30);
}

TEST(TupleTests, Tuple3_EqualityTrue) {
    Tuple3<int, int, int> a(1, 2, 3);
    Tuple3<int, int, int> b(1, 2, 3);
    EXPECT_TRUE(a == b);
}

TEST(TupleTests, Tuple3_EqualityFalse_DifferentItem3) {
    Tuple3<int, int, int> a(1, 2, 3);
    Tuple3<int, int, int> b(1, 2, 9);
    EXPECT_FALSE(a == b);
}

TEST(TupleTests, Tuple3_InequalityTrue) {
    Tuple3<int, int, int> a(1, 2, 3);
    Tuple3<int, int, int> b(4, 5, 6);
    EXPECT_TRUE(a != b);
}

TEST(TupleTests, Tuple3_InequalityFalse_SameValues) {
    Tuple3<int, int, int> a(7, 8, 9);
    Tuple3<int, int, int> b(7, 8, 9);
    EXPECT_FALSE(a != b);
}

TEST(TupleTests, Tuple3_StringIntBool) {
    Tuple3<std::string, int, bool> t("x", 42, false);
    EXPECT_EQ(t.getItem1Property(), "x");
    EXPECT_EQ(t.getItem2Property(), 42);
    EXPECT_FALSE(t.getItem3Property());
}

// ---------------------------------------------------------------------------
// Tuple4
// ---------------------------------------------------------------------------

TEST(TupleTests, Tuple4_ConstructAndAccessItems) {
    Tuple4<int, int, int, int> t(1, 2, 3, 4);
    EXPECT_EQ(t.getItem1Property(), 1);
    EXPECT_EQ(t.getItem2Property(), 2);
    EXPECT_EQ(t.getItem3Property(), 3);
    EXPECT_EQ(t.getItem4Property(), 4);
}

TEST(TupleTests, Tuple4_MixedTypes) {
    Tuple4<std::string, int, double, bool> t("hello", 7, 2.5, true);
    EXPECT_EQ(t.getItem1Property(), "hello");
    EXPECT_EQ(t.getItem2Property(), 7);
    EXPECT_DOUBLE_EQ(t.getItem3Property(), 2.5);
    EXPECT_TRUE(t.getItem4Property());
}

TEST(TupleTests, Tuple4_AllZero) {
    Tuple4<int, int, int, int> t(0, 0, 0, 0);
    EXPECT_EQ(t.getItem1Property(), 0);
    EXPECT_EQ(t.getItem2Property(), 0);
    EXPECT_EQ(t.getItem3Property(), 0);
    EXPECT_EQ(t.getItem4Property(), 0);
}

// ===========================================================================
// TupleExtensions — Deconstruct
// ===========================================================================

TEST(TupleExtensionsTests, Deconstruct_Tuple2) {
    System::Tuple2<int, std::string> t(42, "hello");
    int a; std::string b;
    t.Deconstruct(a, b);
    EXPECT_EQ(a, 42);
    EXPECT_EQ(b, "hello");
}

TEST(TupleExtensionsTests, Deconstruct_Tuple3) {
    System::Tuple3<int, double, std::string> t(1, 2.5, "x");
    int a; double b; std::string c;
    t.Deconstruct(a, b, c);
    EXPECT_EQ(a, 1);
    EXPECT_DOUBLE_EQ(b, 2.5);
    EXPECT_EQ(c, "x");
}

TEST(TupleExtensionsTests, Deconstruct_Tuple4) {
    System::Tuple4<int, int, int, int> t(1, 2, 3, 4);
    int a, b, c, d;
    t.Deconstruct(a, b, c, d);
    EXPECT_EQ(a, 1); EXPECT_EQ(b, 2); EXPECT_EQ(c, 3); EXPECT_EQ(d, 4);
}

// ===========================================================================
// TupleExtensions — ToValueTuple / ToTuple
// ===========================================================================

TEST(TupleExtensionsTests, ToValueTuple_Tuple2) {
    System::Tuple2<int, std::string> t(7, "hi");
    auto vt = System::TupleExtensions::ToValueTuple(t);
    EXPECT_EQ(std::get<0>(vt), 7);
    EXPECT_EQ(std::get<1>(vt), "hi");
}

TEST(TupleExtensionsTests, ToValueTuple_Tuple3) {
    System::Tuple3<int, int, int> t(1, 2, 3);
    auto vt = System::TupleExtensions::ToValueTuple(t);
    EXPECT_EQ(std::get<2>(vt), 3);
}

TEST(TupleExtensionsTests, ToTuple_StdTuple2) {
    auto st = std::make_tuple(10, std::string("world"));
    auto t = System::TupleExtensions::ToTuple(st);
    EXPECT_EQ(t.getItem1Property(), 10);
    EXPECT_EQ(t.getItem2Property(), "world");
}

TEST(TupleExtensionsTests, ToTuple_StdTuple3) {
    auto st = std::make_tuple(1, 2, 3);
    auto t = System::TupleExtensions::ToTuple(st);
    EXPECT_EQ(t.getItem3Property(), 3);
}

TEST(TupleExtensionsTests, ToTuple_StdTuple4) {
    auto st = std::make_tuple(1, 2, 3, 4);
    auto t = System::TupleExtensions::ToTuple(st);
    EXPECT_EQ(t.getItem4Property(), 4);
}

TEST(TupleExtensionsTests, RoundTrip_Tuple2_ToValueTuple_ToTuple) {
    System::Tuple2<int, int> original(5, 6);
    auto vt = System::TupleExtensions::ToValueTuple(original);
    auto back = System::TupleExtensions::ToTuple(vt);
    EXPECT_EQ(back, original);
}

// ---------------------------------------------------------------------------
// CompareTo / operator< (element-by-element, first non-zero wins)
// ---------------------------------------------------------------------------

TEST(TupleCompareTests, Tuple1_CompareTo) {
    System::Tuple1<int> a(1), b(2);
    EXPECT_LT(a.CompareTo(b), 0);
    EXPECT_GT(b.CompareTo(a), 0);
    EXPECT_EQ(a.CompareTo(a), 0);
    EXPECT_TRUE(a < b);
}

TEST(TupleCompareTests, Tuple2_FirstItemDecides) {
    System::Tuple2<int, int> a(1, 100), b(2, 0);
    EXPECT_LT(a.CompareTo(b), 0);
    EXPECT_TRUE(a < b);
}

TEST(TupleCompareTests, Tuple2_FallsThroughToSecondItem) {
    System::Tuple2<int, int> a(5, 1), b(5, 2);
    EXPECT_LT(a.CompareTo(b), 0);
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}

TEST(TupleCompareTests, Tuple3_LexicographicOrdering) {
    System::Tuple3<int, int, int> a(1, 2, 3), b(1, 2, 4);
    EXPECT_LT(a.CompareTo(b), 0);
    EXPECT_EQ(a.CompareTo(a), 0);
}

// ---------------------------------------------------------------------------
// CCF-004 / SR-AUD-062 — tupleHashCombine defined arithmetic (ticket #1831)
//
// `detail::tupleHashCombine` used to evaluate `((h1 << 5) + h1) ^ h2` in signed
// `intcs`. The addition is undefined behaviour whenever it is not representable, and
// `detail::tupleHash` masks element hashes to the low 31 bits, so ANY element hash of
// 2^26 or more reaches it — the audited input was `Tuple2<intcs,intcs>(0x03ffffff, 0)`.
// The helper now evaluates the whole expression in `uintcs`.
//
// This is CCF-004 class A: the repair must not change ANY value. Every literal below
// is the value measured before the change (build-probe/1831_prefix_values.log), so a
// future edit that alters the algorithm — rather than only its arithmetic domain —
// fails here instead of silently rehashing every tuple in every consumer.
// ---------------------------------------------------------------------------

TEST(TupleHashDefinedArithmeticTests, CombineIsUnchangedForTheAuditedOverflowInput) {
    // 0x03ffffff << 5 == 0x7fffffe0; + 0x03ffffff overflowed intcs before the fix.
    EXPECT_EQ(System::detail::tupleHashCombine(0x03ffffff, 0), -2080374817);
    Tuple2<SharpRuntime::intcs, SharpRuntime::intcs> t(0x03ffffff, 0);
    EXPECT_EQ(t.GetHashCode(), -2080374817);
}

TEST(TupleHashDefinedArithmeticTests, CombineIsUnchangedAtBothSignedExtremes) {
    // Largest operand detail::tupleHash can produce (its mask is 0x7fffffff).
    EXPECT_EQ(System::detail::tupleHashCombine(SharpRuntime::INTCS_MAX, 0), 2147483615);
    // Negative h1 is reachable through Tuple8's unmasked Rest.GetHashCode().
    EXPECT_EQ(System::detail::tupleHashCombine(SharpRuntime::INTCS_MIN, -1), 2147483647);
    EXPECT_EQ(System::detail::tupleHashCombine(-2000000000, 0), -1575490560);
    // Ordinary, non-extreme operands.
    EXPECT_EQ(System::detail::tupleHashCombine(1, 2), 35);
    EXPECT_EQ(System::detail::tupleHashCombine(0, 0), 0);
}

TEST(TupleHashDefinedArithmeticTests, EveryArityKeepsTheHashValueItHadBeforeTheFix) {
    EXPECT_EQ((System::Tuple1<SharpRuntime::intcs>(7).GetHashCode()), 7);
    EXPECT_EQ((Tuple2<SharpRuntime::intcs, SharpRuntime::intcs>(1, 2).GetHashCode()), 35);
    EXPECT_EQ((Tuple2<SharpRuntime::intcs, SharpRuntime::intcs>(
                   SharpRuntime::INTCS_MAX, SharpRuntime::INTCS_MAX).GetHashCode()), 32);
    EXPECT_EQ(System::Tuple::Create(1, 2, 3).GetHashCode(), 1152);
    EXPECT_EQ(System::Tuple::Create(1, 2, 3, 4).GetHashCode(), 1252);
    EXPECT_EQ(System::Tuple::Create(1, 2, 3, 4, 5).GetHashCode(), 41313);
    EXPECT_EQ(System::Tuple::Create(1, 2, 3, 4, 5, 6).GetHashCode(), 41415);
    EXPECT_EQ(System::Tuple::Create(1, 2, 3, 4, 5, 6, 7).GetHashCode(), 46176);
    EXPECT_EQ(System::Tuple::Create(1, 2, 3, 4, 5, 6, 7, 8).GetHashCode(), 46216);
}

TEST(TupleHashDefinedArithmeticTests, OverflowingElementHashPropagatesUnchangedThroughEveryArity) {
    // The outer combine overflows on the INNER result even when the later items are 0,
    // so this covers the cascade the single-value case above cannot.
    EXPECT_EQ(System::Tuple::Create(0x03ffffff, 0, 0).GetHashCode(), 67107775);
    EXPECT_EQ(System::Tuple::Create(0x03ffffff, 0, 0, 0, 0, 0, 0,
                                    SharpRuntime::INTCS_MAX).GetHashCode(), -67072928);
}

TEST(TupleHashDefinedArithmeticTests, EqualTuplesStillHashEquallyAcrossOverflowingInputs) {
    // The property that actually matters to a consumer, asserted for the overflowing
    // inputs specifically and for a non-integer element type whose std::hash value is
    // platform-dependent and therefore deliberately not pinned above.
    Tuple2<SharpRuntime::intcs, SharpRuntime::intcs> a(0x03ffffff, 0), b(0x03ffffff, 0);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
    // A neighbouring input is a different tuple -- asserted on equality, not on hash codes. The
    // hash inequality that used to stand here claimed the defined arithmetic cannot collide,
    // which is not a property it has (docs/HashAssertionContractRule.md R2); the arithmetic
    // itself is pinned by exact value in the two tests above.
    EXPECT_FALSE(a == (Tuple2<SharpRuntime::intcs, SharpRuntime::intcs>(0x03fffffe, 0)));

    Tuple2<std::string, std::string> s1("alpha", "beta"), s2("alpha", "beta");
    EXPECT_EQ(s1.GetHashCode(), s2.GetHashCode());

    auto big1 = System::Tuple::Create(0x7fffffff, 0x7ffffffe, 0x7ffffffd, 0x7ffffffc);
    auto big2 = System::Tuple::Create(0x7fffffff, 0x7ffffffe, 0x7ffffffd, 0x7ffffffc);
    EXPECT_EQ(big1.GetHashCode(), big2.GetHashCode());
}

TEST(TupleHashDefinedArithmeticTests, HelperRemainsNoexcept) {
    // Plan section 8: the class A repair must not introduce a throw. `noexcept` is part
    // of the public shape a consumer may already rely on.
    static_assert(noexcept(System::detail::tupleHashCombine(0, 0)));
    SUCCEED();
}

// ---------------------------------------------------------------------------
// #2330 / SR-AUD-063 — Tuple's elements are getter-only
// ---------------------------------------------------------------------------

TEST(TupleContractPinTests, Fix2330_TheElementsAreGetterOnlyAsInDotNet) {
    // .NET's TupleN holds PRIVATE READONLY fields behind getter-only properties; this port
    // published them as public mutable data members, so `Tuple::Create(1, 2).Item1 = 99`
    // compiled and stuck. Under CLAUDE.md rule 5 the accessor is getItem1Property().
    const auto t = System::Tuple::Create(1, std::string("two"));
    EXPECT_EQ(1, t.getItem1Property());
    EXPECT_EQ("two", t.getItem2Property());

    // Getter-only in the C++ sense that matters: the accessor hands back a CONST reference, so a
    // caller cannot write through it either. A `T&` return would have satisfied rule 5 while
    // leaving the finding intact.
    static_assert(std::is_same_v<decltype(t.getItem1Property()), const int&>,
                  "#2330: the accessor must return a const reference, not a mutable one");
    static_assert(std::is_same_v<decltype(System::Tuple::Create(1).getItem1Property()), const int&>,
                  "#2330: and that holds for a temporary too");
}

TEST(TupleContractPinTests, Fix2330_EveryArityMovedIncludingTuple8sRest) {
    // All eight arities were changed, and Tuple8 also publishes Rest -- the member most easily
    // forgotten, because it is the only one not called ItemN.
    const auto t8 = System::Tuple::Create(1, 2, 3, 4, 5, 6, 7, 8);
    EXPECT_EQ(1, t8.getItem1Property());
    EXPECT_EQ(7, t8.getItem7Property());
    EXPECT_EQ(8, t8.getRestProperty().getItem1Property())
        << "Rest is a nested Tuple1, so it needs the accessor at both levels";

    EXPECT_EQ(3, System::Tuple::Create(1, 2, 3).getItem3Property());
    EXPECT_EQ(4, System::Tuple::Create(1, 2, 3, 4).getItem4Property());
    EXPECT_EQ(5, System::Tuple::Create(1, 2, 3, 4, 5).getItem5Property());
    EXPECT_EQ(6, System::Tuple::Create(1, 2, 3, 4, 5, 6).getItem6Property());
}

TEST(TupleContractPinTests, Decl2330_ValueTupleDeliberatelyKeepsItsPublicFields) {
    // THE BOUNDARY, pinned so a later sweep cannot "finish the job" by mistake. .NET's ValueTuple
    // is a struct with PUBLIC MUTABLE FIELDS -- `public T1 Item1;` -- where Tuple is a class with
    // getter-only properties. The two differ deliberately in .NET, so they differ deliberately
    // here, and ValueTuple was left untouched by #2330.
    auto vt = System::ValueTuple1<int>(42);
    EXPECT_EQ(42, vt.Item1);
    vt.Item1 = 7;               // legal, and must stay legal
    EXPECT_EQ(7, vt.Item1);
}
