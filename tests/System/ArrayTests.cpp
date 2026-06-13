// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <vector>
#include "System/Array.hpp"

using System::Array;
using SharpRuntime::intcs;

// ---------------------------------------------------------------------------
// Sort — default comparer
// ---------------------------------------------------------------------------

TEST(ArrayTests, Sort_Ascending_Integer) {
    std::vector<int> v = {3, 1, 4, 1, 5, 9, 2, 6};
    Array::Sort(v);
    EXPECT_TRUE(std::is_sorted(v.begin(), v.end()));
}

TEST(ArrayTests, Sort_EmptyVector_NoOp) {
    std::vector<int> v;
    Array::Sort(v);
    EXPECT_TRUE(v.empty());
}

TEST(ArrayTests, Sort_SingleElement_NoOp) {
    std::vector<int> v = {42};
    Array::Sort(v);
    EXPECT_EQ(v[0], 42);
}

TEST(ArrayTests, Sort_AlreadySorted_StaysOrdered) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    Array::Sort(v);
    EXPECT_EQ(v, (std::vector<int>{1, 2, 3, 4, 5}));
}

// ---------------------------------------------------------------------------
// Sort — custom comparison
// ---------------------------------------------------------------------------

TEST(ArrayTests, Sort_Descending_CustomComparison) {
    std::vector<int> v = {3, 1, 4, 1, 5};
    Array::Sort<int>(v, [](const int& a, const int& b) { return b - a; });
    EXPECT_TRUE(std::is_sorted(v.begin(), v.end(), std::greater<int>()));
}

TEST(ArrayTests, Sort_ByStringLength) {
    std::vector<std::string> v = {"banana", "fig", "apple", "kiwi"};
    Array::Sort<std::string>(v, [](const std::string& a, const std::string& b) {
        return static_cast<int>(a.size()) - static_cast<int>(b.size());
    });
    EXPECT_LE(v[0].size(), v[1].size());
    EXPECT_LE(v[1].size(), v[2].size());
    EXPECT_LE(v[2].size(), v[3].size());
}

// ---------------------------------------------------------------------------
// Copy — vector overload
// ---------------------------------------------------------------------------

TEST(ArrayTests, Copy_VectorSubrange) {
    std::vector<int> src = {10, 20, 30, 40, 50};
    std::vector<int> dst(5, 0);
    Array::Copy(src, 1, dst, 2, 3);
    EXPECT_EQ(dst[2], 20);
    EXPECT_EQ(dst[3], 30);
    EXPECT_EQ(dst[4], 40);
    EXPECT_EQ(dst[0], 0);  // untouched
    EXPECT_EQ(dst[1], 0);  // untouched
}

TEST(ArrayTests, Copy_VectorFullRange) {
    std::vector<int> src = {1, 2, 3};
    std::vector<int> dst(3, 0);
    Array::Copy(src, 0, dst, 0, 3);
    EXPECT_EQ(dst, src);
}

TEST(ArrayTests, Copy_VectorZeroLength_NoChange) {
    std::vector<int> src = {9, 9, 9};
    std::vector<int> dst = {1, 2, 3};
    Array::Copy(src, 0, dst, 0, 0);
    EXPECT_EQ(dst, (std::vector<int>{1, 2, 3}));
}

// ---------------------------------------------------------------------------
// Copy — raw pointer overload
// ---------------------------------------------------------------------------

TEST(ArrayTests, Copy_RawPointer_CopiesBytes) {
    int src[4] = {10, 20, 30, 40};
    int dst[4] = {0, 0, 0, 0};
    Array::Copy(src, 1, dst, 0, 3);
    EXPECT_EQ(dst[0], 20);
    EXPECT_EQ(dst[1], 30);
    EXPECT_EQ(dst[2], 40);
}

// ---------------------------------------------------------------------------
// Resize
// ---------------------------------------------------------------------------

TEST(ArrayTests, Resize_GrowVector_NewElementsAreZero) {
    std::vector<int> v = {1, 2, 3};
    Array::Resize(v, 5);
    EXPECT_EQ(static_cast<intcs>(v.size()), 5);
    EXPECT_EQ(v[3], 0);
    EXPECT_EQ(v[4], 0);
}

TEST(ArrayTests, Resize_ShrinkVector_TruncatesElements) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    Array::Resize(v, 2);
    EXPECT_EQ(static_cast<intcs>(v.size()), 2);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
}

TEST(ArrayTests, Resize_SameSize_NoChange) {
    std::vector<int> v = {7, 8};
    Array::Resize(v, 2);
    EXPECT_EQ(v, (std::vector<int>{7, 8}));
}

// ---------------------------------------------------------------------------
// IndexOf
// ---------------------------------------------------------------------------

TEST(ArrayTests, IndexOf_PresentElement_ReturnsCorrectIndex) {
    std::vector<int> v = {10, 20, 30, 40};
    EXPECT_EQ(Array::IndexOf(v, 30), 2);
}

TEST(ArrayTests, IndexOf_FirstElement_ReturnsZero) {
    std::vector<int> v = {5, 10, 15};
    EXPECT_EQ(Array::IndexOf(v, 5), 0);
}

TEST(ArrayTests, IndexOf_LastElement_ReturnsLastIndex) {
    std::vector<int> v = {5, 10, 15};
    EXPECT_EQ(Array::IndexOf(v, 15), 2);
}

TEST(ArrayTests, IndexOf_AbsentElement_ReturnsMinusOne) {
    std::vector<int> v = {1, 2, 3};
    EXPECT_EQ(Array::IndexOf(v, 99), -1);
}

TEST(ArrayTests, IndexOf_EmptyVector_ReturnsMinusOne) {
    std::vector<int> v;
    EXPECT_EQ(Array::IndexOf(v, 0), -1);
}

TEST(ArrayTests, IndexOf_DuplicateValues_ReturnsFirst) {
    std::vector<int> v = {7, 7, 7};
    EXPECT_EQ(Array::IndexOf(v, 7), 0);
}

// ---------------------------------------------------------------------------
// Reverse
// ---------------------------------------------------------------------------

TEST(ArrayTests, Reverse_OddLength) {
    std::vector<int> v = {1, 2, 3};
    Array::Reverse(v);
    EXPECT_EQ(v, (std::vector<int>{3, 2, 1}));
}

TEST(ArrayTests, Reverse_EvenLength) {
    std::vector<int> v = {1, 2, 3, 4};
    Array::Reverse(v);
    EXPECT_EQ(v, (std::vector<int>{4, 3, 2, 1}));
}

TEST(ArrayTests, Reverse_SingleElement_Unchanged) {
    std::vector<int> v = {42};
    Array::Reverse(v);
    EXPECT_EQ(v[0], 42);
}

TEST(ArrayTests, Reverse_EmptyVector_NoOp) {
    std::vector<int> v;
    Array::Reverse(v);
    EXPECT_TRUE(v.empty());
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

TEST(ArrayTests, Clear_MiddleRange_ZeroesTargetElements) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    Array::Clear(v, 1, 3);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 0);
    EXPECT_EQ(v[2], 0);
    EXPECT_EQ(v[3], 0);
    EXPECT_EQ(v[4], 5);
}

TEST(ArrayTests, Clear_EntireVector) {
    std::vector<int> v = {9, 9, 9};
    Array::Clear(v, 0, 3);
    EXPECT_EQ(v, (std::vector<int>{0, 0, 0}));
}

TEST(ArrayTests, Clear_ZeroLength_NoChange) {
    std::vector<int> v = {5, 6, 7};
    Array::Clear(v, 0, 0);
    EXPECT_EQ(v, (std::vector<int>{5, 6, 7}));
}

// --- BinarySearch ---

TEST(ArrayTests, BinarySearch_Found_ReturnsIndex) {
    std::vector<int> v = {1, 3, 5, 7, 9};
    EXPECT_EQ(Array::BinarySearch(v, 5), 2);
}

TEST(ArrayTests, BinarySearch_FirstElement) {
    std::vector<int> v = {1, 3, 5, 7, 9};
    EXPECT_EQ(Array::BinarySearch(v, 1), 0);
}

TEST(ArrayTests, BinarySearch_LastElement) {
    std::vector<int> v = {1, 3, 5, 7, 9};
    EXPECT_EQ(Array::BinarySearch(v, 9), 4);
}

TEST(ArrayTests, BinarySearch_NotFound_NegativeComplement) {
    std::vector<int> v = {1, 3, 5, 7, 9};
    int r = Array::BinarySearch(v, 4);
    EXPECT_LT(r, 0); // not found → negative
}

TEST(ArrayTests, BinarySearch_EmptyVector_ReturnsNegative) {
    std::vector<int> v;
    EXPECT_LT(Array::BinarySearch(v, 1), 0);
}

// --- Fill ---

TEST(ArrayTests, Fill_EntireVector) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    Array::Fill(v, 0);
    EXPECT_EQ(v, (std::vector<int>{0, 0, 0, 0, 0}));
}

TEST(ArrayTests, Fill_Range) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    Array::Fill(v, 9, 1, 3);
    EXPECT_EQ(v, (std::vector<int>{1, 9, 9, 9, 5}));
}

TEST(ArrayTests, Fill_StringVector) {
    std::vector<std::string> v(3);
    Array::Fill(v, std::string("x"));
    EXPECT_EQ(v[0], "x");
    EXPECT_EQ(v[2], "x");
}

// ---------------------------------------------------------------------------
// Empty<T>
// ---------------------------------------------------------------------------

TEST(ArrayTests, Empty_Int_IsEmpty) {
    auto v = Array::Empty<int>();
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}
TEST(ArrayTests, Empty_String_IsEmpty) {
    auto v = Array::Empty<std::string>();
    EXPECT_TRUE(v.empty());
}
TEST(ArrayTests, Empty_ReturnsSeparateInstances) {
    auto a = Array::Empty<int>();
    auto b = Array::Empty<int>();
    a.push_back(1);
    EXPECT_TRUE(b.empty());
}
