// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cstddef>
#include <stdexcept>
#include <vector>
#include "System/ReadOnlySpan.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/detail/SpanLength.hpp"

using System::ReadOnlySpan;
using System::Span;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(ReadOnlySpanTests, DefaultCtor_IsEmpty) {
    ReadOnlySpan<int> s;
    EXPECT_EQ(s.getLengthProperty(), 0);
    EXPECT_TRUE(s.getIsEmptyProperty());
}

TEST(ReadOnlySpanTests, PtrLengthCtor_StoresValues) {
    int arr[] = {1, 2, 3};
    ReadOnlySpan<int> s(arr, 3);
    EXPECT_EQ(s.getLengthProperty(), 3);
    EXPECT_FALSE(s.getIsEmptyProperty());
}

TEST(ReadOnlySpanTests, VectorCtor_CoversFull) {
    std::vector<int> v = {10, 20, 30};
    ReadOnlySpan<int> s(v);
    EXPECT_EQ(s.getLengthProperty(), 3);
    EXPECT_EQ(s[0], 10);
    EXPECT_EQ(s[2], 30);
}

TEST(ReadOnlySpanTests, FromSpan_ImplicitConversion) {
    std::vector<int> v = {5, 6, 7};
    Span<int> sp(v.data(), static_cast<int>(v.size()));
    ReadOnlySpan<int> ros = sp;
    EXPECT_EQ(ros.getLengthProperty(), 3);
    EXPECT_EQ(ros[1], 6);
}

// ---------------------------------------------------------------------------
// Element access
// ---------------------------------------------------------------------------

TEST(ReadOnlySpanTests, Indexer_ReturnsCorrectElement) {
    int arr[] = {7, 8, 9};
    ReadOnlySpan<int> s(arr, 3);
    EXPECT_EQ(s[0], 7);
    EXPECT_EQ(s[2], 9);
}

TEST(ReadOnlySpanTests, Indexer_OutOfRange_Throws) {
    int arr[] = {1};
    ReadOnlySpan<int> s(arr, 1);
    EXPECT_THROW((void)s[1], System::IndexOutOfRangeException);
    EXPECT_THROW((void)s[-1], System::IndexOutOfRangeException);
}

// Real .NET's Span<T>/ReadOnlySpan<T> indexer throws IndexOutOfRangeException, distinct from
// List<T>'s ArgumentOutOfRangeException convention -- verifies the writable Span<T> overload
// (both const and non-const operator[]) matches, not just ReadOnlySpan<T>.
TEST(SpanTests, Indexer_OutOfRange_ThrowsIndexOutOfRangeException) {
    int arr[] = {1};
    Span<int> s(arr, 1);
    EXPECT_THROW((void)s[1], System::IndexOutOfRangeException);
    EXPECT_THROW((void)s[-1], System::IndexOutOfRangeException);

    const Span<int> cs(arr, 1);
    EXPECT_THROW((void)cs[1], System::IndexOutOfRangeException);
    EXPECT_THROW((void)cs[-1], System::IndexOutOfRangeException);
}

TEST(ReadOnlySpanTests, GetPointer_ReturnsFirstElement) {
    int arr[] = {42};
    ReadOnlySpan<int> s(arr, 1);
    EXPECT_EQ(s.getPointer(), arr);
}

// ---------------------------------------------------------------------------
// Iteration
// ---------------------------------------------------------------------------

TEST(ReadOnlySpanTests, RangeFor_IteratesAllElements) {
    int arr[] = {1, 2, 3, 4};
    ReadOnlySpan<int> s(arr, 4);
    int sum = 0;
    for (int v : s) sum += v;
    EXPECT_EQ(sum, 10);
}

// ---------------------------------------------------------------------------
// Slicing
// ---------------------------------------------------------------------------

TEST(ReadOnlySpanTests, Slice_FromStart) {
    int arr[] = {1, 2, 3, 4, 5};
    ReadOnlySpan<int> s(arr, 5);
    auto sl = s.Slice(2);
    EXPECT_EQ(sl.getLengthProperty(), 3);
    EXPECT_EQ(sl[0], 3);
}

TEST(ReadOnlySpanTests, Slice_StartAndLength) {
    int arr[] = {1, 2, 3, 4, 5};
    ReadOnlySpan<int> s(arr, 5);
    auto sl = s.Slice(1, 3);
    EXPECT_EQ(sl.getLengthProperty(), 3);
    EXPECT_EQ(sl[0], 2);
    EXPECT_EQ(sl[2], 4);
}

// Same overflow-bypasses-the-check bug as Span<T>::Slice(intcs,intcs) (ticket 265) -- fixed
// alongside it since ReadOnlySpan<T> duplicates the same Slice(start,length) logic.
TEST(ReadOnlySpanTests, Slice_StartPlusLengthOverflow_ThrowsInsteadOfBypassingCheck) {
    int arr[] = {1, 2, 3, 4, 5};
    ReadOnlySpan<int> s(arr, 5);
    EXPECT_THROW(s.Slice(2147483647, 10), System::ArgumentOutOfRangeException);
}

TEST(ReadOnlySpanTests, Slice_OutOfRange_Throws) {
    int arr[] = {1, 2};
    ReadOnlySpan<int> s(arr, 2);
    EXPECT_THROW((void)s.Slice(3), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)s.Slice(0, 5), System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// CopyTo / TryCopyTo
// ---------------------------------------------------------------------------

TEST(ReadOnlySpanTests, CopyTo_CopiesElements) {
    int src[] = {1, 2, 3};
    ReadOnlySpan<int> ros(src, 3);
    std::vector<int> dst(3, 0);
    Span<int> sp(dst.data(), 3);
    ros.CopyTo(sp);
    EXPECT_EQ(dst[0], 1);
    EXPECT_EQ(dst[2], 3);
}

TEST(ReadOnlySpanTests, CopyTo_DestinationTooShort_Throws) {
    int src[] = {1, 2, 3};
    ReadOnlySpan<int> ros(src, 3);
    std::vector<int> dst(2, 0);
    Span<int> sp(dst.data(), 2);
    EXPECT_THROW(ros.CopyTo(sp), System::ArgumentException);
}

TEST(ReadOnlySpanTests, TryCopyTo_Success) {
    int src[] = {10, 20};
    ReadOnlySpan<int> ros(src, 2);
    std::vector<int> dst(2, 0);
    Span<int> sp(dst.data(), 2);
    EXPECT_TRUE(ros.TryCopyTo(sp));
    EXPECT_EQ(dst[0], 10);
}

TEST(ReadOnlySpanTests, TryCopyTo_DestinationTooShort_ReturnsFalse) {
    int src[] = {1, 2, 3};
    ReadOnlySpan<int> ros(src, 3);
    std::vector<int> dst(1, 0);
    Span<int> sp(dst.data(), 1);
    EXPECT_FALSE(ros.TryCopyTo(sp));
}

// ---------------------------------------------------------------------------
// ToArray
// ---------------------------------------------------------------------------

TEST(ReadOnlySpanTests, ToArray_CreatesVector) {
    int arr[] = {4, 5, 6};
    ReadOnlySpan<int> s(arr, 3);
    auto v = s.ToArray();
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 4);
    EXPECT_EQ(v[2], 6);
}

TEST(ReadOnlySpanTests, ToArray_EmptySpan_ReturnsEmptyVector) {
    ReadOnlySpan<int> s;
    EXPECT_TRUE(s.ToArray().empty());
}

// ---------------------------------------------------------------------------
// ToString
// ---------------------------------------------------------------------------

TEST(ReadOnlySpanTests, ToString_CharSpan_ReturnsString) {
    const char* txt = "hello";
    ReadOnlySpan<char> s(txt, 5);
    EXPECT_EQ(s.ToString(), "hello");
}

TEST(ReadOnlySpanTests, ToString_IntSpan_ReturnsDescriptor) {
    int arr[] = {1, 2};
    ReadOnlySpan<int> s(arr, 2);
    EXPECT_NE(s.ToString().find("2"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Equality operators (pointer + length, NOT content)
// ---------------------------------------------------------------------------

TEST(ReadOnlySpanTests, EqualityOp_SamePointerSameLength_Equal) {
    int arr[] = {1, 2, 3};
    ReadOnlySpan<int> a(arr, 3);
    ReadOnlySpan<int> b(arr, 3);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(ReadOnlySpanTests, EqualityOp_DifferentLength_NotEqual) {
    int arr[] = {1, 2, 3};
    ReadOnlySpan<int> a(arr, 3);
    ReadOnlySpan<int> b(arr, 2);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(ReadOnlySpanTests, EqualityOp_DifferentPointer_NotEqual) {
    int a[] = {1, 2};
    int b[] = {1, 2};
    ReadOnlySpan<int> sa(a, 2);
    ReadOnlySpan<int> sb(b, 2);
    EXPECT_FALSE(sa == sb);
}

// ---------------------------------------------------------------------------
// Empty static
// ---------------------------------------------------------------------------

TEST(ReadOnlySpanTests, Empty_IsEmpty) {
    auto e = ReadOnlySpan<int>::Empty();
    EXPECT_TRUE(e.getIsEmptyProperty());
    EXPECT_EQ(e.getLengthProperty(), 0);
}

// ---------------------------------------------------------------------------
// SR-AUD-043a (#1852): the pointer/length constructors reject a negative length
// before it can become a huge size_t and drive an unbounded raw read. .NET keeps
// _length a signed int and validates at construction; the port does the same and
// keeps intcs length_ (no layout change). A zero length stays valid.
// ---------------------------------------------------------------------------

TEST(ReadOnlySpanTests, PtrLengthCtor_NegativeLength_ThrowsAOORE) {
    int arr[] = {1, 2, 3};
    EXPECT_THROW(ReadOnlySpan<int>(arr, -1), System::ArgumentOutOfRangeException);
}

TEST(SpanTests, PtrLengthCtor_NegativeLength_ThrowsAOORE) {
    int arr[] = {1, 2, 3};
    EXPECT_THROW(Span<int>(arr, -1), System::ArgumentOutOfRangeException);
}

TEST(ReadOnlySpanTests, PtrLengthCtor_IntMinLength_ThrowsAOORE) {
    int arr[] = {1};
    EXPECT_THROW(ReadOnlySpan<int>(arr, SharpRuntime::INTCS_MIN), System::ArgumentOutOfRangeException);
}

TEST(ReadOnlySpanTests, PtrLengthCtor_ZeroLength_Constructs) {
    int arr[] = {1};
    ReadOnlySpan<int> s(arr, 0);
    EXPECT_EQ(s.getLengthProperty(), 0);
    EXPECT_TRUE(s.getIsEmptyProperty());
}

TEST(SpanTests, PtrLengthCtor_ZeroLength_Constructs) {
    int arr[] = {1};
    Span<int> s(arr, 0);
    EXPECT_EQ(s.getLengthProperty(), 0);
}

// The reachable exploit: a ReadOnlySpan<uint8_t> with a negative length used to be
// constructible and could reach HashCode::AddBytes as an unbounded raw read. It can
// no longer be constructed, so that path is closed at the source.
TEST(ReadOnlySpanTests, NegativeLengthByteSpan_CannotBeConstructed) {
    std::uint8_t one = 0x42;
    EXPECT_THROW(ReadOnlySpan<std::uint8_t>(&one, -1), System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// SR-AUD-043a (#1852): the vector constructors narrow size() to a signed intcs.
// checkedSpanLength is the shared guard that rejects a size() > INT32_MAX instead
// of silently producing a negative length. Tested directly with a synthetic size
// so the guard is exercised without allocating a 2.1-billion-element container.
// ---------------------------------------------------------------------------

TEST(SpanLengthTests, CheckedSpanLength_WithinRange_ReturnsNarrowed) {
    EXPECT_EQ(System::detail::checkedSpanLength(0, "v"), 0);
    EXPECT_EQ(System::detail::checkedSpanLength(123, "v"), 123);
    EXPECT_EQ(System::detail::checkedSpanLength(
                  static_cast<std::size_t>(SharpRuntime::INTCS_MAX), "v"),
              SharpRuntime::INTCS_MAX);
}

TEST(SpanLengthTests, CheckedSpanLength_OverInt32Max_ThrowsAOORE) {
    const std::size_t oversized = static_cast<std::size_t>(SharpRuntime::INTCS_MAX) + 1;
    EXPECT_THROW(System::detail::checkedSpanLength(oversized, "v"),
                 System::ArgumentOutOfRangeException);
}

TEST(SpanLengthTests, CheckedSpanLength_MaxSizeT_ThrowsAOORE) {
    EXPECT_THROW(System::detail::checkedSpanLength(static_cast<std::size_t>(-1), "v"),
                 System::ArgumentOutOfRangeException);
}

// The empty-vector constructors still work (size 0 narrows cleanly).
TEST(SpanTests, VectorCtor_Empty_Constructs) {
    std::vector<int> v;
    Span<int> s(v);
    EXPECT_EQ(s.getLengthProperty(), 0);
}

TEST(ReadOnlySpanTests, VectorCtor_Empty_Constructs) {
    std::vector<int> v;
    ReadOnlySpan<int> s(v);
    EXPECT_EQ(s.getLengthProperty(), 0);
}
