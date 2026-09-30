// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>
#include "System/Collections/Generic/Dictionary.hpp"

namespace {
using System::Collections::Generic::Dictionary;
using D = Dictionary<std::string, int>;
D MakeDictionary() {
    D result;
    for (const char* key : {"help", "cls", "echo", "remote", "pos", "fps", "tr"})
        result.Add(key, result.getCountProperty());
    return result;
}
std::vector<std::string> Enumerate(const D& dictionary) {
    std::vector<std::string> result;
    for (const auto& entry : dictionary) result.push_back(entry.first);
    return result;
}
TEST(DictionaryEntryOrder, FreshKeysAndValuesFollowDotNetEntrySlots) {
    const auto d = MakeDictionary();
    EXPECT_EQ(Enumerate(d), (std::vector<std::string>{"help","cls","echo","remote","pos","fps","tr"}));
    EXPECT_EQ(d.getKeysProperty(), Enumerate(d));
    EXPECT_EQ(d.getValuesProperty(), (std::vector<int>{0,1,2,3,4,5,6}));
}
TEST(DictionaryEntryOrder, ReusesRemovedSlotsLastInFirstOutLikeDotNet4) {
    auto d = MakeDictionary();
    ASSERT_TRUE(d.Remove("cls"));
    int value = -1;
    ASSERT_TRUE(d.Remove("remote", value));
    ASSERT_EQ(value, 3);
    ASSERT_TRUE(d.TryAdd("new-a", 9));
    d["new-b"] = 10;
    EXPECT_EQ(Enumerate(d), (std::vector<std::string>{"help","new-b","echo","new-a","pos","fps","tr"}));
    EXPECT_EQ(d.getValuesProperty(), (std::vector<int>{0,10,2,9,4,5,6}));
}
TEST(DictionaryEntryOrder, RehashDoesNotReorderAndTrimCompactsLiveSlots) {
    auto d = MakeDictionary();
    d.Remove("cls");
    d.EnsureCapacity(1000);
    EXPECT_EQ(Enumerate(d), (std::vector<std::string>{"help","echo","remote","pos","fps","tr"}));
    d.TrimExcess();
    d.Add("next", 20);
    EXPECT_EQ(Enumerate(d), (std::vector<std::string>{"help","echo","remote","pos","fps","tr","next"}));
}
TEST(DictionaryEntryOrder, ClearStartsWithNoOldSlots) {
    auto d = MakeDictionary();
    d.Remove("cls"); d.Clear();
    d.Add("second", 2); d.Add("first", 1);
    EXPECT_EQ(Enumerate(d), (std::vector<std::string>{"second","first"}));
}
TEST(DictionaryEntryOrder, CopyMoveAndAssignmentOwnIndependentSlots) {
    auto original = MakeDictionary(); original.Remove("cls");
    auto copy = original;
    D moved(std::move(copy));
    D assigned; assigned = moved;
    moved.Add("reused", 7);
    EXPECT_EQ(Enumerate(original), Enumerate(assigned));
    EXPECT_NE(Enumerate(original), Enumerate(moved));
}
TEST(DictionaryEntryOrder, MutableIteratorWritesValueWithoutReordering) {
    auto d = MakeDictionary(); auto it = d.begin();
    it->second = 99;
    d["help"] = 100;
    EXPECT_EQ(it->first, "help"); EXPECT_EQ(it->second, 100);
    EXPECT_EQ(Enumerate(d).front(), "help");
}
TEST(DictionaryEntryOrder, StructuralMutationsStillFailFastIncludingArrow) {
    auto d = MakeDictionary(); auto it = d.begin();
    d.Remove("help");
    EXPECT_THROW((void)*it, System::InvalidOperationException);
    EXPECT_THROW((void)it->first, System::InvalidOperationException);
    EXPECT_THROW(++it, System::InvalidOperationException);
}
TEST(DictionaryEntryOrder, RawMapInteropReconcilesRemovedAndInsertedKeys) {
    auto d = MakeDictionary(); auto& map = d.ToMap();
    map.erase("cls"); map.emplace("raw", 5);
    const auto keys = d.getKeysProperty();
    EXPECT_EQ(keys.size(), map.size());
    EXPECT_EQ(keys[1], "raw");
    EXPECT_EQ(d.getValuesProperty()[1], 5);
}
TEST(DictionaryEntryOrder, PublishedMapIteratorConstructorStillAcceptsRawEntries) {
    D d;
    auto& map = d.ToMap();
    map.emplace("raw", 3);
    D::iterator it(&d, map.begin());
    EXPECT_EQ(it->first, "raw");
    EXPECT_EQ(it->second, 3);
}
TEST(DictionaryEntryOrder, FloatingKeyPolicyIncludesNaNInOneLiveSlot) {
    Dictionary<double, int> d;
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    d.Add(nan, 1); d.Add(2.0, 2); d.Remove(nan); d.Add(nan, 3);
    EXPECT_EQ(d.getValuesProperty(), (std::vector<int>{3,2}));
    EXPECT_THROW(d.Add(nan, 4), System::ArgumentException);
}
TEST(DictionaryEntryOrder, LayoutGrowthRequiresAFullConsumerRebuild) {
    if constexpr (sizeof(void*) == 8) {
        // Before: Dictionary<int,int> 64/8; iterator 24/8 (prior inventory pin).
        EXPECT_EQ(sizeof(Dictionary<int,int>), 176u);
        EXPECT_EQ(alignof(Dictionary<int,int>), 8u);
        EXPECT_EQ(sizeof(Dictionary<int,int>::iterator), 24u);
        EXPECT_EQ(alignof(Dictionary<int,int>::iterator), 8u);
    }
}
}
