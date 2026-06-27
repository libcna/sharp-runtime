// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for Batch 19:
//   ReadOnlyObservableCollection: CollectionChanged forwarding, Empty, Count, Contains
//   ReadOnlySet: Empty, Count, IsEmpty, Contains, set operations
//   BitVector32: Equals, GetHashCode, ToString(value), Section equality/hash/ToString
#include <gtest/gtest.h>
#include "System/Collections/ObjectModel/ReadOnlyObservableCollection.hpp"
#include "System/Collections/ObjectModel/ReadOnlySet.hpp"
#include "System/Collections/Specialized/BitVector32.hpp"
#include <memory>
#include <string>
#include <unordered_set>

using System::Collections::ObjectModel::ObservableCollection;
using System::Collections::ObjectModel::ReadOnlyObservableCollection;
using System::Collections::ObjectModel::ReadOnlySet;
using System::Collections::Specialized::BitVector32;

// ===========================================================================
// ReadOnlyObservableCollection
// ===========================================================================

TEST(ReadOnlyObservableCollectionBatch19Test, CountAndIndexer) {
    ObservableCollection<int> src;
    src.Add(10); src.Add(20); src.Add(30);
    ReadOnlyObservableCollection<int> roc(std::move(src));
    EXPECT_EQ(roc.getCountProperty(), 3);
    EXPECT_EQ(roc[1], 20);
}

TEST(ReadOnlyObservableCollectionBatch19Test, IsEmpty_True) {
    ObservableCollection<int> empty;
    ReadOnlyObservableCollection<int> roc(empty);
    EXPECT_TRUE(roc.getIsEmptyProperty());
}

TEST(ReadOnlyObservableCollectionBatch19Test, IsEmpty_False) {
    ObservableCollection<int> src;
    src.Add(1);
    ReadOnlyObservableCollection<int> roc(std::move(src));
    EXPECT_FALSE(roc.getIsEmptyProperty());
}

TEST(ReadOnlyObservableCollectionBatch19Test, ContainsAndIndexOf) {
    ObservableCollection<std::string> src;
    src.Add("alpha"); src.Add("beta");
    ReadOnlyObservableCollection<std::string> roc(std::move(src));
    EXPECT_TRUE(roc.Contains("beta"));
    EXPECT_FALSE(roc.Contains("gamma"));
    EXPECT_EQ(roc.IndexOf("alpha"), 0);
    EXPECT_EQ(roc.IndexOf("missing"), -1);
}

TEST(ReadOnlyObservableCollectionBatch19Test, Empty_IsEmpty) {
    auto& e = ReadOnlyObservableCollection<int>::Empty();
    EXPECT_EQ(e.getCountProperty(), 0);
    EXPECT_TRUE(e.getIsEmptyProperty());
}

TEST(ReadOnlyObservableCollectionBatch19Test, STLIteration) {
    ObservableCollection<int> src;
    src.Add(1); src.Add(2); src.Add(3);
    ReadOnlyObservableCollection<int> roc(std::move(src));
    int sum = 0;
    for (const auto& v : roc) sum += v;
    EXPECT_EQ(sum, 6);
}

// ===========================================================================
// ReadOnlySet
// ===========================================================================

TEST(ReadOnlySetBatch19Test, CountAndContains) {
    auto s = std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2, 3});
    ReadOnlySet<int> rs(s);
    EXPECT_EQ(rs.getCountProperty(), 3);
    EXPECT_TRUE(rs.Contains(2));
    EXPECT_FALSE(rs.Contains(99));
}

TEST(ReadOnlySetBatch19Test, IsEmpty) {
    auto empty = std::make_shared<std::unordered_set<int>>();
    ReadOnlySet<int> rs(empty);
    EXPECT_TRUE(rs.getIsEmptyProperty());
    auto s = std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1});
    ReadOnlySet<int> rs2(s);
    EXPECT_FALSE(rs2.getIsEmptyProperty());
}

TEST(ReadOnlySetBatch19Test, EmptyStatic) {
    auto e = ReadOnlySet<int>::Empty();
    EXPECT_EQ(e.getCountProperty(), 0);
    EXPECT_TRUE(e.getIsEmptyProperty());
}

TEST(ReadOnlySetBatch19Test, IsSubsetOf) {
    auto a = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2}));
    auto b = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2, 3}));
    EXPECT_TRUE(a.IsSubsetOf(b));
    EXPECT_FALSE(b.IsSubsetOf(a));
}

TEST(ReadOnlySetBatch19Test, IsSupersetOf) {
    auto sup = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2, 3}));
    auto sub = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{2, 3}));
    EXPECT_TRUE(sup.IsSupersetOf(sub));
    EXPECT_FALSE(sub.IsSupersetOf(sup));
}

TEST(ReadOnlySetBatch19Test, IsProperSubsetOf) {
    auto sub = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2}));
    auto sup = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2, 3}));
    auto eq  = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2}));
    EXPECT_TRUE(sub.IsProperSubsetOf(sup));
    EXPECT_FALSE(sub.IsProperSubsetOf(eq));
}

TEST(ReadOnlySetBatch19Test, IsProperSupersetOf) {
    auto sup = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2, 3}));
    auto sub = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{2, 3}));
    auto eq  = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2, 3}));
    EXPECT_TRUE(sup.IsProperSupersetOf(sub));
    EXPECT_FALSE(sup.IsProperSupersetOf(eq));
}

TEST(ReadOnlySetBatch19Test, Overlaps) {
    auto a = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2}));
    auto b = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{2, 3}));
    auto c = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{4, 5}));
    EXPECT_TRUE(a.Overlaps(b));
    EXPECT_FALSE(a.Overlaps(c));
}

TEST(ReadOnlySetBatch19Test, SetEquals) {
    auto a = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2, 3}));
    auto b = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{3, 1, 2}));
    auto c = ReadOnlySet<int>(std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{1, 2}));
    EXPECT_TRUE(a.SetEquals(b));
    EXPECT_FALSE(a.SetEquals(c));
}

TEST(ReadOnlySetBatch19Test, STLIteration) {
    auto s = std::make_shared<std::unordered_set<int>>(std::initializer_list<int>{10, 20, 30});
    ReadOnlySet<int> rs(s);
    int sum = 0;
    for (const auto& v : rs) sum += v;
    EXPECT_EQ(sum, 60);
}

// ===========================================================================
// BitVector32
// ===========================================================================

TEST(BitVector32Batch19Test, Equals_SameData) {
    BitVector32 a(42), b(42);
    EXPECT_TRUE(a.Equals(b));
    EXPECT_TRUE(a == b);
}

TEST(BitVector32Batch19Test, Equals_DiffData) {
    BitVector32 a(1), b(2);
    EXPECT_FALSE(a.Equals(b));
    EXPECT_TRUE(a != b);
}

TEST(BitVector32Batch19Test, GetHashCode_MatchesData) {
    BitVector32 bv(0xDEAD);
    EXPECT_EQ(bv.GetHashCode(), static_cast<int>(0xDEAD));
}

TEST(BitVector32Batch19Test, ToStringStatic) {
    BitVector32 bv(0);
    std::string s = BitVector32::ToString(bv);
    EXPECT_EQ(s, bv.ToString());
    EXPECT_EQ(s.substr(0, 10), "BitVector3");
}

TEST(BitVector32Batch19Test, Section_Equals) {
    BitVector32::Section s1(0x0F, 4);
    BitVector32::Section s2(0x0F, 4);
    BitVector32::Section s3(0x0F, 8);
    EXPECT_TRUE(s1.Equals(s2));
    EXPECT_TRUE(s1 == s2);
    EXPECT_FALSE(s1.Equals(s3));
    EXPECT_TRUE(s1 != s3);
}

TEST(BitVector32Batch19Test, Section_GetHashCode_Stable) {
    BitVector32::Section s(0x0F, 4);
    EXPECT_EQ(s.GetHashCode(), s.GetHashCode());
    BitVector32::Section s2(0x0F, 4);
    EXPECT_EQ(s.GetHashCode(), s2.GetHashCode());
}

TEST(BitVector32Batch19Test, Section_ToString) {
    BitVector32::Section s(3, 2);
    std::string t = s.ToString();
    EXPECT_NE(t.find("3"), std::string::npos);
    EXPECT_NE(t.find("2"), std::string::npos);
    EXPECT_EQ(BitVector32::Section::ToString(s), t);
}

TEST(BitVector32Batch19Test, CreateMaskSequence) {
    int m1 = BitVector32::CreateMask();
    int m2 = BitVector32::CreateMask(m1);
    int m3 = BitVector32::CreateMask(m2);
    EXPECT_EQ(m1, 1);
    EXPECT_EQ(m2, 2);
    EXPECT_EQ(m3, 4);
}

TEST(BitVector32Batch19Test, BitFlags) {
    BitVector32 bv;
    int m1 = BitVector32::CreateMask();
    int m2 = BitVector32::CreateMask(m1);
    bv.set(m1, true);
    EXPECT_TRUE(bv[m1]);
    EXPECT_FALSE(bv[m2]);
    bv.set(m1, false);
    EXPECT_FALSE(bv[m1]);
}

TEST(BitVector32Batch19Test, SectionGetSet) {
    BitVector32 bv;
    auto s1 = BitVector32::CreateSection(7);
    auto s2 = BitVector32::CreateSection(15, s1);
    bv.set(s1, 5);
    bv.set(s2, 12);
    EXPECT_EQ(bv[s1], 5);
    EXPECT_EQ(bv[s2], 12);
}
