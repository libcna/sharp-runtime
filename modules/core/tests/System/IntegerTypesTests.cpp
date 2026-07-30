// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
// Tests for: Int16, Int32, Int128, IntPtr
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include "System/Int16.hpp"
#include "System/Int32.hpp"
#include "System/Int128.hpp"
#include "System/IntPtr.hpp"

// ---------------------------------------------------------------------------
// Int16
// ---------------------------------------------------------------------------
TEST(Int16Tests, MaxValue_Is32767) {
    EXPECT_EQ(System::Int16::MaxValue, 32767);
}

TEST(Int16Tests, MinValue_IsMinus32768) {
    EXPECT_EQ(System::Int16::MinValue, -32768);
}

TEST(Int16Tests, Parse_ValidString) {
    EXPECT_EQ(System::Int16::Parse("100"), 100);
}

TEST(Int16Tests, TryParse_Valid_ReturnsTrue) {
    SharpRuntime::shortcs r = 0;
    EXPECT_TRUE(System::Int16::TryParse("42", r));
    EXPECT_EQ(r, 42);
}

TEST(Int16Tests, TryParse_Invalid_ReturnsFalse) {
    SharpRuntime::shortcs r = 0;
    EXPECT_FALSE(System::Int16::TryParse("abc", r));
}

TEST(Int16Tests, ToString_Default) {
    EXPECT_EQ(System::Int16::ToString(255), "255");
}

TEST(Int16Tests, ToString_HexFormat) {
    EXPECT_EQ(System::Int16::ToString(255, "X"), "FF");
}

TEST(Int16Tests, ToString_MalformedWidth_ThrowsFormatException) {
    EXPECT_THROW(System::Int16::ToString(5, "Xz"), System::FormatException);
    EXPECT_THROW(System::Int16::ToString(5, "X99999999999999999999"), System::FormatException);
}

// ---------------------------------------------------------------------------
// Int32
// ---------------------------------------------------------------------------
TEST(Int32Tests2, MaxValue_IsCorrect) {
    EXPECT_EQ(System::Int32::MaxValue, 2147483647);
}

TEST(Int32Tests2, MinValue_IsCorrect) {
    EXPECT_EQ(System::Int32::MinValue, -2147483648LL);
}

TEST(Int32Tests2, Parse_ValidString) {
    EXPECT_EQ(System::Int32::Parse("12345"), 12345);
}

TEST(Int32Tests2, TryParse_Valid) {
    SharpRuntime::intcs r = 0;
    EXPECT_TRUE(System::Int32::TryParse("99", r));
    EXPECT_EQ(r, 99);
}

TEST(Int32Tests2, TryParse_Invalid_ReturnsFalse) {
    SharpRuntime::intcs r = 0;
    EXPECT_FALSE(System::Int32::TryParse("xyz", r));
}

// ---------------------------------------------------------------------------
// Int128 (basic smoke test — may be stub)
// ---------------------------------------------------------------------------
TEST(Int128Tests2, DefaultCtor_DoesNotThrow) {
    EXPECT_NO_THROW(System::Int128 v);
}

TEST(Int128Tests2, Abs_PositiveValue_ReturnsSameValue) {
    System::Int128 v(static_cast<__int128>(5));
    EXPECT_TRUE(System::Int128::Abs(v) == v);
}

TEST(Int128Tests2, Abs_NegativeValue_ReturnsPositive) {
    System::Int128 v(static_cast<__int128>(-5));
    EXPECT_TRUE(System::Int128::Abs(v) == System::Int128(static_cast<__int128>(5)));
}

// ---------------------------------------------------------------------------
// IntPtr
// ---------------------------------------------------------------------------
TEST(IntPtrTests2, Zero_IsZero) {
    System::IntPtr p = System::IntPtr::Zero;
    EXPECT_TRUE(p.IsZero());
}

TEST(IntPtrTests2, Ctor_FromInt) {
    System::IntPtr p(42);
    EXPECT_FALSE(p.IsZero());
    EXPECT_EQ(static_cast<intptr_t>(p), 42);
}

// ===========================================================================
// Ticket #1832 -- CCF-004 / SR-AUD-025: defined native-width wrap in IntPtr
// ===========================================================================
//
// IntPtr::Add evaluated `pointer.value + offset` in intptr_t and Subtract the
// mirror, so the two native boundary cases were signed-overflow UB. Measured in
// build-probe/1829_ccf004_survey.log:
//
//   case 3  IntPtr.hpp:105: signed integer overflow: 9223372036854775807 + 1
//   case 4  IntPtr.hpp:114: signed integer overflow: -9223372036854775808 - 1
//
// .NET exposes these as `pointer + offset` / `pointer - offset` for nint under
// ordinary UNCHECKED C# arithmetic, whose modulo-native-width wrap is DEFINED.
// Both now compute in uintptr_t. This is CCF-004 class A -- no observable change --
// so the values asserted below are the ones measured BEFORE the repair.

TEST(IntPtrTests2, Add_AtMaxValue_WrapsToMinValue) {
    const auto r = System::IntPtr::Add(System::IntPtr::MaxValue, 1);
    EXPECT_EQ(r.ToInt64(), System::IntPtr::MinValue.ToInt64());
}

TEST(IntPtrTests2, Subtract_AtMinValue_WrapsToMaxValue) {
    const auto r = System::IntPtr::Subtract(System::IntPtr::MinValue, 1);
    EXPECT_EQ(r.ToInt64(), System::IntPtr::MaxValue.ToInt64());
}

// A NEGATIVE offset is the case the two-step cast in the implementation exists for.
// intcs is 32-bit and uintptr_t is 64-bit on LP64, so the offset must be
// sign-extended to intptr_t before widening; converting straight to uintptr_t would
// zero-extend and turn Add(p, -1) into an addition of 4294967295 -- a wrong answer,
// not merely a differently-arrived-at one.
TEST(IntPtrTests2, Add_NegativeOffset_SubtractsRatherThanZeroExtending) {
    const System::IntPtr p(1000);
    EXPECT_EQ(System::IntPtr::Add(p, -1).ToInt64(), 999);
    EXPECT_EQ(System::IntPtr::Add(p, -1000).ToInt64(), 0);
    EXPECT_EQ(System::IntPtr::Add(p, -2000).ToInt64(), -1000);
}

TEST(IntPtrTests2, Subtract_NegativeOffset_AddsRatherThanZeroExtending) {
    const System::IntPtr p(1000);
    EXPECT_EQ(System::IntPtr::Subtract(p, -1).ToInt64(), 1001);
    EXPECT_EQ(System::IntPtr::Subtract(p, -2000).ToInt64(), 3000);
}

TEST(IntPtrTests2, AddAndSubtract_OrdinaryPositiveOffsets) {
    const System::IntPtr p(100);
    EXPECT_EQ(System::IntPtr::Add(p, 0).ToInt64(), 100);
    EXPECT_EQ(System::IntPtr::Add(p, 23).ToInt64(), 123);
    EXPECT_EQ(System::IntPtr::Subtract(p, 0).ToInt64(), 100);
    EXPECT_EQ(System::IntPtr::Subtract(p, 40).ToInt64(), 60);
}

// The friend operator+ / operator- forms forward to the two methods above, so they
// are covered transitively. Pinned independently so a refactor that stops
// forwarding cannot silently reintroduce a signed overflow on this path.
TEST(IntPtrTests2, OperatorForms_MatchTheNamedMethodsAtTheBoundaries) {
    EXPECT_EQ((System::IntPtr::MaxValue + 1).ToInt64(), System::IntPtr::MinValue.ToInt64());
    EXPECT_EQ((System::IntPtr::MinValue - 1).ToInt64(), System::IntPtr::MaxValue.ToInt64());
    const System::IntPtr p(500);
    EXPECT_EQ((p + -1).ToInt64(), 499);
    EXPECT_EQ((p - -1).ToInt64(), 501);
    EXPECT_EQ((p + 7).ToInt64(), 507);
    EXPECT_EQ((p - 7).ToInt64(), 493);
}

// The extreme offset in both directions, so the widening is exercised at its limits.
TEST(IntPtrTests2, AddAndSubtract_ExtremeOffsets) {
    const System::IntPtr zero = System::IntPtr::Zero;
    EXPECT_EQ(System::IntPtr::Add(zero, 2147483647).ToInt64(), 2147483647LL);
    EXPECT_EQ(System::IntPtr::Add(zero, -2147483647 - 1).ToInt64(), -2147483648LL);
    EXPECT_EQ(System::IntPtr::Subtract(zero, 2147483647).ToInt64(), -2147483647LL);
    EXPECT_EQ(System::IntPtr::Subtract(zero, -2147483647 - 1).ToInt64(), 2147483648LL);
}

// ---------------------------------------------------------------------------
// CCF-004 / SR-AUD-019 — Int128 MinValue parse and format (ticket #1834)
//
// Two public paths negated a SIGNED `__int128` whose magnitude 2^127 is not
// representable, which is undefined behaviour:
//   * `TryParse`  (Int128.hpp:234) converted the unsigned magnitude to signed and then
//     negated it — reached by the MinValue decimal string;
//   * `ToString`  (Int128.hpp:143) negated `value_` to build its decimal magnitude —
//     reached by `MinValue().ToString()` and by every format that delegates to it.
// Both now stay in `unsigned __int128` and convert once at the end, the same idiom
// `operator-()` in that header already used.
//
// CCF-004 class A: no value may change. Every expectation below is the value measured
// BEFORE the change (build-probe/1834_prefix_values.log).
// ---------------------------------------------------------------------------

namespace {
    constexpr const char* kInt128MinDecimal = "-170141183460469231731687303715884105728";
    constexpr const char* kInt128MaxDecimal = "170141183460469231731687303715884105727";
}

TEST(Int128DefinedArithmeticTests, MinValueFormatsToItsExactDecimalForm) {
    EXPECT_EQ(System::Int128::MinValue().ToString(), kInt128MinDecimal);
    EXPECT_EQ(System::Int128::MaxValue().ToString(), kInt128MaxDecimal);
}

TEST(Int128DefinedArithmeticTests, MinValueParsesToExactlyMinValue) {
    System::Int128 v;
    ASSERT_TRUE(System::Int128::TryParse(kInt128MinDecimal, v));
    EXPECT_TRUE(v == System::Int128::MinValue());
    // And through the throwing entry point, which forwards to the same code.
    EXPECT_TRUE(System::Int128::Parse(kInt128MinDecimal) == System::Int128::MinValue());
}

TEST(Int128DefinedArithmeticTests, MinValueRoundTripsThroughFormatAndParse) {
    System::Int128 back;
    ASSERT_TRUE(System::Int128::TryParse(System::Int128::MinValue().ToString(), back));
    EXPECT_TRUE(back == System::Int128::MinValue());
    ASSERT_TRUE(System::Int128::TryParse(System::Int128::MaxValue().ToString(), back));
    EXPECT_TRUE(back == System::Int128::MaxValue());
}

TEST(Int128DefinedArithmeticTests, EveryFormatPathThatDelegatesToToStringIsUnchanged) {
    const auto mn = System::Int128::MinValue();
    EXPECT_EQ(mn.ToString("D"), kInt128MinDecimal);
    EXPECT_EQ(mn.ToString("G"), kInt128MinDecimal);
    // Width padding inserts zeros AFTER the sign, which the negative magnitude path feeds.
    EXPECT_EQ(mn.ToString("D40"), "-0170141183460469231731687303715884105728");
    EXPECT_EQ(mn.ToString("D45"), "-000000170141183460469231731687303715884105728");
    // The hex path never negated and must stay untouched.
    EXPECT_EQ(mn.ToString("X"), "80000000000000000000000000000000");
    EXPECT_EQ(mn.ToString("x"), "80000000000000000000000000000000");
}

TEST(Int128DefinedArithmeticTests, BothDomainEndpointsAndTheirNeighboursParseAsBefore) {
    System::Int128 v;
    // MinValue + 1 and MaxValue: still accepted.
    EXPECT_TRUE(System::Int128::TryParse("-170141183460469231731687303715884105727", v));
    EXPECT_EQ(v.ToString(), "-170141183460469231731687303715884105727");
    EXPECT_TRUE(System::Int128::TryParse(kInt128MaxDecimal, v));
    // One past each end: still rejected, so the unsigned negation did not widen the domain.
    EXPECT_FALSE(System::Int128::TryParse("-170141183460469231731687303715884105729", v));
    EXPECT_FALSE(System::Int128::TryParse("170141183460469231731687303715884105728", v));
}

TEST(Int128DefinedArithmeticTests, OrdinaryAndZeroSignedValuesAreUnchanged) {
    System::Int128 v;
    // A negative zero magnitude is the degenerate case of the changed expression.
    ASSERT_TRUE(System::Int128::TryParse("-0", v));
    EXPECT_TRUE(v == System::Int128(0));
    EXPECT_EQ(v.ToString(), "0");

    ASSERT_TRUE(System::Int128::TryParse("-1", v));
    EXPECT_EQ(v.ToString(), "-1");
    ASSERT_TRUE(System::Int128::TryParse("-42", v));
    EXPECT_EQ(v.ToString(), "-42");
    ASSERT_TRUE(System::Int128::TryParse("42", v));
    EXPECT_EQ(v.ToString(), "42");
    EXPECT_EQ(System::Int128(0).ToString(), "0");
    EXPECT_EQ(System::Int128(static_cast<__int128>(-42)).ToString(), "-42");
}

TEST(Int128DefinedArithmeticTests, MalformedInputIsStillRejected) {
    System::Int128 v;
    EXPECT_FALSE(System::Int128::TryParse("", v));
    EXPECT_FALSE(System::Int128::TryParse("-", v));
    EXPECT_FALSE(System::Int128::TryParse("+", v));
    EXPECT_FALSE(System::Int128::TryParse("-1x", v));
    EXPECT_THROW((void)System::Int128::Parse("-1x"), System::FormatException);
}

TEST(Int128DefinedArithmeticTests, TheExistingOverflowOperatorVectorsStillHold) {
    // Plan section 10.5: SR-AUD-019 is the parse/format paths, NOT the operators. These
    // assertions exist so this ticket cannot silently disturb them.
    const auto mn = System::Int128::MinValue();
    const auto mx = System::Int128::MaxValue();
    EXPECT_TRUE(-mn == mn);                                  // unchecked negation wraps
    EXPECT_TRUE(mn - System::Int128(1) == mx);
    EXPECT_TRUE(mx + System::Int128(1) == mn);
    EXPECT_THROW((void)System::Int128::Abs(mn), System::OverflowException);
    EXPECT_THROW((void)(mn / System::Int128(-1)), System::OverflowException);
    EXPECT_THROW((void)(mn % System::Int128(-1)), System::OverflowException);
}
