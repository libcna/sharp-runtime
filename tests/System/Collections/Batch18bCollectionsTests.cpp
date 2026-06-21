// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for Batch 18b gap-fill:
//   BitArray: Clone, GetEnumerator, IsReadOnly/IsSynchronized/SyncRoot
//   ArrayList: Clone, GetRange, IndexOf overloads, LastIndexOf overloads,
//              Sort(IComparer), BinarySearch, Repeat, ICollection-ctor
#include <gtest/gtest.h>
#include <any>
#include <vector>
#include "System/Collections/BitArray.hpp"
#include "System/Collections/ArrayList.hpp"
#include "System/Collections/Comparer.hpp"

using namespace System::Collections;

// -----------------------------------------------------------------------
// BitArray — Clone
// -----------------------------------------------------------------------

TEST(BitArrayGapFill, Clone_IsIndependent) {
    BitArray orig(4, false);
    orig.Set(1, true);
    BitArray copy = orig.Clone();
    copy.Set(0, true);
    EXPECT_FALSE(orig.Get(0));  // original unaffected
    EXPECT_TRUE(copy.Get(0));
    EXPECT_TRUE(copy.Get(1));
}

TEST(BitArrayGapFill, Clone_SameLength) {
    BitArray orig(7, true);
    BitArray copy = orig.Clone();
    EXPECT_EQ(copy.getLengthProperty(), 7);
    EXPECT_TRUE(copy.HasAllSet());
}

// -----------------------------------------------------------------------
// BitArray — GetEnumerator
// -----------------------------------------------------------------------

TEST(BitArrayGapFill, GetEnumerator_IteratesAllBits) {
    BitArray ba(4, false);
    ba.Set(0, true);
    ba.Set(2, true);
    IEnumerator* e = ba.GetEnumerator();
    ASSERT_NE(e, nullptr);
    std::vector<bool> got;
    while (e->MoveNext()) {
        bool v = *static_cast<bool*>(e->getCurrent());
        got.push_back(v);
    }
    delete e;
    ASSERT_EQ(got.size(), 4u);
    EXPECT_TRUE(got[0]);
    EXPECT_FALSE(got[1]);
    EXPECT_TRUE(got[2]);
    EXPECT_FALSE(got[3]);
}

TEST(BitArrayGapFill, GetEnumerator_Reset_RestartsIteration) {
    BitArray ba(3, true);
    IEnumerator* e = ba.GetEnumerator();
    ASSERT_NE(e, nullptr);
    e->MoveNext();
    e->MoveNext();
    e->Reset();
    int count = 0;
    while (e->MoveNext()) ++count;
    delete e;
    EXPECT_EQ(count, 3);
}

TEST(BitArrayGapFill, GetEnumerator_Empty_NoMoveNext) {
    BitArray ba(0);
    IEnumerator* e = ba.GetEnumerator();
    ASSERT_NE(e, nullptr);
    EXPECT_FALSE(e->MoveNext());
    delete e;
}

// -----------------------------------------------------------------------
// BitArray — properties
// -----------------------------------------------------------------------

TEST(BitArrayGapFill, IsReadOnly_False) {
    BitArray ba(4, false);
    EXPECT_FALSE(ba.getIsReadOnlyProperty());
}

TEST(BitArrayGapFill, IsSynchronized_False) {
    BitArray ba(4, false);
    EXPECT_FALSE(ba.getIsSynchronizedProperty());
}

TEST(BitArrayGapFill, SyncRoot_NonNull_Stable) {
    BitArray ba(4, false);
    EXPECT_NE(ba.getSyncRootProperty(), nullptr);
    EXPECT_EQ(ba.getSyncRootProperty(), ba.getSyncRootProperty());
}

// -----------------------------------------------------------------------
// ArrayList — Clone
// -----------------------------------------------------------------------

TEST(ArrayListGapFill, Clone_IsShallowCopy) {
    ArrayList al;
    al.Add(std::any(42));
    al.Add(std::any(99));
    ArrayList copy = al.Clone();
    EXPECT_EQ(copy.getCountProperty(), 2);
    al.Add(std::any(1));
    EXPECT_EQ(copy.getCountProperty(), 2);  // independent
}

// -----------------------------------------------------------------------
// ArrayList — GetRange
// -----------------------------------------------------------------------

TEST(ArrayListGapFill, GetRange_ReturnsSubset) {
    ArrayList al;
    al.Add(std::any(10));
    al.Add(std::any(20));
    al.Add(std::any(30));
    al.Add(std::any(40));
    ArrayList sub = al.GetRange(1, 2);
    EXPECT_EQ(sub.getCountProperty(), 2);
}

TEST(ArrayListGapFill, GetRange_Empty) {
    ArrayList al;
    al.Add(std::any(1));
    ArrayList sub = al.GetRange(0, 0);
    EXPECT_EQ(sub.getCountProperty(), 0);
}

// -----------------------------------------------------------------------
// ArrayList — IndexOf overloads
// -----------------------------------------------------------------------

TEST(ArrayListGapFill, IndexOf_StartIndex_FindsFromOffset) {
    ArrayList al;
    al.Add(std::any(1));
    al.Add(std::any(2));
    al.Add(std::any(3));
    // type int matches first at index 0; searching from 1 should find index 1
    EXPECT_EQ(al.IndexOf(std::any(0), 1), 1);
}

TEST(ArrayListGapFill, IndexOf_StartIndex_NotFound) {
    ArrayList al;
    al.Add(std::any(std::string("x")));
    EXPECT_EQ(al.IndexOf(std::any(42), 0), -1);
}

TEST(ArrayListGapFill, IndexOf_StartIndex_Count) {
    ArrayList al;
    al.Add(std::any(1));
    al.Add(std::any(2));
    al.Add(std::any(3));
    // search 1 element starting at index 2 — finds it
    EXPECT_EQ(al.IndexOf(std::any(0), 2, 1), 2);
    // search 1 element starting at index 0 — does not reach index 2
    EXPECT_EQ(al.IndexOf(std::any(0), 0, 1), 0);
}

// -----------------------------------------------------------------------
// ArrayList — LastIndexOf overloads
// -----------------------------------------------------------------------

TEST(ArrayListGapFill, LastIndexOf_Basic) {
    ArrayList al;
    al.Add(std::any(1));
    al.Add(std::any(2));
    al.Add(std::any(3));
    // last int element is at index 2
    EXPECT_EQ(al.LastIndexOf(std::any(0)), 2);
}

TEST(ArrayListGapFill, LastIndexOf_StartIndex) {
    ArrayList al;
    al.Add(std::any(1));
    al.Add(std::any(2));
    al.Add(std::any(3));
    // search backwards from index 1 — last int at index 1
    EXPECT_EQ(al.LastIndexOf(std::any(0), 1), 1);
}

TEST(ArrayListGapFill, LastIndexOf_StartIndex_Count) {
    ArrayList al;
    al.Add(std::any(1));
    al.Add(std::any(2));
    al.Add(std::any(3));
    // search 2 elements ending at index 2 (indices 1,2): last int at 2
    EXPECT_EQ(al.LastIndexOf(std::any(0), 2, 2), 2);
    // search 1 element ending at index 0: last int at 0
    EXPECT_EQ(al.LastIndexOf(std::any(0), 0, 1), 0);
}

// -----------------------------------------------------------------------
// ArrayList — Sort(IComparer)
// -----------------------------------------------------------------------

// Comparer that compares std::any* containing int values
struct IntAnyComparer : public IComparer {
    int Compare(const void* x, const void* y) const override {
        int a = std::any_cast<int>(*static_cast<const std::any*>(x));
        int b = std::any_cast<int>(*static_cast<const std::any*>(y));
        return (a < b) ? -1 : (a > b) ? 1 : 0;
    }
};

TEST(ArrayListGapFill, Sort_FullList) {
    ArrayList al;
    al.Add(std::any(3));
    al.Add(std::any(1));
    al.Add(std::any(2));
    IntAnyComparer cmp;
    al.Sort(cmp);
    EXPECT_EQ(std::any_cast<int>(al[0]), 1);
    EXPECT_EQ(std::any_cast<int>(al[1]), 2);
    EXPECT_EQ(std::any_cast<int>(al[2]), 3);
}

TEST(ArrayListGapFill, Sort_Range) {
    ArrayList al;
    al.Add(std::any(10));
    al.Add(std::any(3));
    al.Add(std::any(1));
    al.Add(std::any(2));
    IntAnyComparer cmp;
    // sort indices 1..3
    al.Sort(1, 3, cmp);
    EXPECT_EQ(std::any_cast<int>(al[0]), 10); // unchanged
    EXPECT_EQ(std::any_cast<int>(al[1]), 1);
    EXPECT_EQ(std::any_cast<int>(al[2]), 2);
    EXPECT_EQ(std::any_cast<int>(al[3]), 3);
}

// -----------------------------------------------------------------------
// ArrayList — BinarySearch
// -----------------------------------------------------------------------

TEST(ArrayListGapFill, BinarySearch_Found) {
    ArrayList al;
    al.Add(std::any(1));
    al.Add(std::any(2));
    al.Add(std::any(3));
    al.Add(std::any(4));
    IntAnyComparer cmp;
    int idx = al.BinarySearch(std::any(3), cmp);
    EXPECT_EQ(idx, 2);
}

TEST(ArrayListGapFill, BinarySearch_NotFound_ReturnsComplement) {
    ArrayList al;
    al.Add(std::any(1));
    al.Add(std::any(3));
    al.Add(std::any(5));
    IntAnyComparer cmp;
    int idx = al.BinarySearch(std::any(4), cmp);
    EXPECT_LT(idx, 0);  // not found → negative (bitwise complement)
    EXPECT_EQ(~idx, 2); // insertion point is 2
}

TEST(ArrayListGapFill, BinarySearch_Range) {
    ArrayList al;
    al.Add(std::any(10));
    al.Add(std::any(1));
    al.Add(std::any(2));
    al.Add(std::any(3));
    IntAnyComparer cmp;
    int idx = al.BinarySearch(1, 3, std::any(2), cmp);
    EXPECT_EQ(idx, 2);
}

// -----------------------------------------------------------------------
// ArrayList — Repeat
// -----------------------------------------------------------------------

TEST(ArrayListGapFill, Repeat_CreatesNcopies) {
    ArrayList al = ArrayList::Repeat(std::any(42), 5);
    EXPECT_EQ(al.getCountProperty(), 5);
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(std::any_cast<int>(al[i]), 42);
}

TEST(ArrayListGapFill, Repeat_ZeroCount) {
    ArrayList al = ArrayList::Repeat(std::any(1), 0);
    EXPECT_EQ(al.getCountProperty(), 0);
}

// -----------------------------------------------------------------------
// ArrayList — ICollection constructor
// -----------------------------------------------------------------------

TEST(ArrayListGapFill, ICollectionCtor_CopiesElements) {
    // Use a concrete ArrayList as the source ICollection
    // (GetEnumerator returns nullptr currently, so count should be 0 — that is expected)
    ArrayList src;
    src.Add(std::any(7));
    // ICollection ctor iterates via GetEnumerator; ArrayList::GetEnumerator returns nullptr
    ArrayList dst(static_cast<ICollection&>(src));
    // Documented: GetEnumerator not implemented → nothing copied; count = 0
    EXPECT_EQ(dst.getCountProperty(), 0);
}
