// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <vector>
#include "System/MemoryExtensions.hpp"

using System::Span;
using System::ReadOnlySpan;
using System::MemoryExtensions;

// ---------------------------------------------------------------------------
// AsSpan
// ---------------------------------------------------------------------------

TEST(MemoryExtensionsTests, AsSpan_Vector_FullLength) {
    std::vector<int> v = {1, 2, 3};
    auto s = MemoryExtensions::AsSpan(v);
    EXPECT_EQ(s.getLengthProperty(), 3);
    EXPECT_EQ(s[0], 1);
    EXPECT_EQ(s[2], 3);
}

TEST(MemoryExtensionsTests, AsSpan_Vector_WithStart) {
    std::vector<int> v = {10, 20, 30, 40};
    auto s = MemoryExtensions::AsSpan(v, 2);
    EXPECT_EQ(s.getLengthProperty(), 2);
    EXPECT_EQ(s[0], 30);
}

TEST(MemoryExtensionsTests, AsSpan_Vector_StartAndLength) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    auto s = MemoryExtensions::AsSpan(v, 1, 3);
    EXPECT_EQ(s.getLengthProperty(), 3);
    EXPECT_EQ(s[0], 2);
    EXPECT_EQ(s[2], 4);
}

// ---------------------------------------------------------------------------
// Contains
// ---------------------------------------------------------------------------

TEST(MemoryExtensionsTests, Contains_Found) {
    std::vector<int> v = {1, 2, 3};
    EXPECT_TRUE(MemoryExtensions::Contains(ReadOnlySpan<int>(v), 2));
}

TEST(MemoryExtensionsTests, Contains_NotFound) {
    std::vector<int> v = {1, 2, 3};
    EXPECT_FALSE(MemoryExtensions::Contains(ReadOnlySpan<int>(v), 99));
}

// ---------------------------------------------------------------------------
// IndexOf
// ---------------------------------------------------------------------------

TEST(MemoryExtensionsTests, IndexOf_Found) {
    std::vector<int> v = {10, 20, 30};
    EXPECT_EQ(MemoryExtensions::IndexOf(ReadOnlySpan<int>(v), 20), 1);
}

TEST(MemoryExtensionsTests, IndexOf_NotFound) {
    std::vector<int> v = {10, 20, 30};
    EXPECT_EQ(MemoryExtensions::IndexOf(ReadOnlySpan<int>(v), 99), -1);
}

TEST(MemoryExtensionsTests, IndexOf_FirstOccurrence) {
    std::vector<int> v = {5, 5, 5};
    EXPECT_EQ(MemoryExtensions::IndexOf(ReadOnlySpan<int>(v), 5), 0);
}

// ---------------------------------------------------------------------------
// LastIndexOf
// ---------------------------------------------------------------------------

TEST(MemoryExtensionsTests, LastIndexOf_Found) {
    std::vector<int> v = {1, 2, 1};
    EXPECT_EQ(MemoryExtensions::LastIndexOf(ReadOnlySpan<int>(v), 1), 2);
}

TEST(MemoryExtensionsTests, LastIndexOf_NotFound) {
    std::vector<int> v = {1, 2, 3};
    EXPECT_EQ(MemoryExtensions::LastIndexOf(ReadOnlySpan<int>(v), 9), -1);
}

// ---------------------------------------------------------------------------
// SequenceEqual
// ---------------------------------------------------------------------------

TEST(MemoryExtensionsTests, SequenceEqual_Equal) {
    std::vector<int> a = {1, 2, 3};
    std::vector<int> b = {1, 2, 3};
    EXPECT_TRUE(MemoryExtensions::SequenceEqual(ReadOnlySpan<int>(a), ReadOnlySpan<int>(b)));
}

TEST(MemoryExtensionsTests, SequenceEqual_DifferentValues) {
    std::vector<int> a = {1, 2, 3};
    std::vector<int> b = {1, 2, 4};
    EXPECT_FALSE(MemoryExtensions::SequenceEqual(ReadOnlySpan<int>(a), ReadOnlySpan<int>(b)));
}

TEST(MemoryExtensionsTests, SequenceEqual_DifferentLength) {
    std::vector<int> a = {1, 2};
    std::vector<int> b = {1, 2, 3};
    EXPECT_FALSE(MemoryExtensions::SequenceEqual(ReadOnlySpan<int>(a), ReadOnlySpan<int>(b)));
}

// ---------------------------------------------------------------------------
// Fill
// ---------------------------------------------------------------------------

TEST(MemoryExtensionsTests, Fill_AllElements) {
    std::vector<int> v(4, 0);
    MemoryExtensions::Fill(Span<int>(v), 7);
    for (int x : v) EXPECT_EQ(x, 7);
}

// ---------------------------------------------------------------------------
// Reverse
// ---------------------------------------------------------------------------

TEST(MemoryExtensionsTests, Reverse_Elements) {
    std::vector<int> v = {1, 2, 3, 4};
    MemoryExtensions::Reverse(Span<int>(v));
    EXPECT_EQ(v[0], 4);
    EXPECT_EQ(v[3], 1);
}

// ---------------------------------------------------------------------------
// CopyTo
// ---------------------------------------------------------------------------

TEST(MemoryExtensionsTests, CopyTo_CopiesElements) {
    std::vector<int> src = {1, 2, 3};
    std::vector<int> dst(3, 0);
    MemoryExtensions::CopyTo(ReadOnlySpan<int>(src), Span<int>(dst));
    EXPECT_EQ(dst[0], 1);
    EXPECT_EQ(dst[2], 3);
}

// ---------------------------------------------------------------------------
// Sort
// ---------------------------------------------------------------------------

TEST(MemoryExtensionsTests, Sort_Ascending) {
    std::vector<int> v = {3, 1, 4, 1, 5};
    MemoryExtensions::Sort(Span<int>(v));
    EXPECT_TRUE(std::is_sorted(v.begin(), v.end()));
}

TEST(MemoryExtensionsTests, Sort_WithComparer) {
    std::vector<int> v = {1, 3, 2};
    MemoryExtensions::Sort(Span<int>(v), std::greater<int>{});
    EXPECT_EQ(v[0], 3);
    EXPECT_EQ(v[2], 1);
}

// ---------------------------------------------------------------------------
// StartsWith / EndsWith
// ---------------------------------------------------------------------------

TEST(MemoryExtensionsTests, StartsWith_True) {
    std::vector<int> v = {1, 2, 3, 4};
    std::vector<int> prefix = {1, 2};
    EXPECT_TRUE(MemoryExtensions::StartsWith(ReadOnlySpan<int>(v), ReadOnlySpan<int>(prefix)));
}

TEST(MemoryExtensionsTests, StartsWith_False) {
    std::vector<int> v = {1, 2, 3};
    std::vector<int> prefix = {2, 3};
    EXPECT_FALSE(MemoryExtensions::StartsWith(ReadOnlySpan<int>(v), ReadOnlySpan<int>(prefix)));
}

TEST(MemoryExtensionsTests, EndsWith_True) {
    std::vector<int> v = {1, 2, 3, 4};
    std::vector<int> suffix = {3, 4};
    EXPECT_TRUE(MemoryExtensions::EndsWith(ReadOnlySpan<int>(v), ReadOnlySpan<int>(suffix)));
}

TEST(MemoryExtensionsTests, EndsWith_False) {
    std::vector<int> v = {1, 2, 3};
    std::vector<int> suffix = {1, 2};
    EXPECT_FALSE(MemoryExtensions::EndsWith(ReadOnlySpan<int>(v), ReadOnlySpan<int>(suffix)));
}

TEST(MemoryExtensionsTests, StartsWith_LongerValue_False) {
    std::vector<int> v = {1};
    std::vector<int> prefix = {1, 2, 3};
    EXPECT_FALSE(MemoryExtensions::StartsWith(ReadOnlySpan<int>(v), ReadOnlySpan<int>(prefix)));
}
