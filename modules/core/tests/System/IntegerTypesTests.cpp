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
