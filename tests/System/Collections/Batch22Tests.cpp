// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for Batch 22:
//   OrderedDictionary:  new ctors, IsReadOnly/IsFixedSize/IsSynchronized/SyncRoot, AsReadOnly,
//                       getKeysProperty/ValuesProperty, mutation guard
//   StringCollection:   IsSynchronized, SyncRoot
//   StringDictionary:   getKeysProperty, getValuesProperty, getSyncRootProperty
#include <gtest/gtest.h>
#include "System/Collections/Specialized/OrderedDictionary.hpp"
#include "System/Collections/Specialized/StringCollection.hpp"
#include "System/Collections/Specialized/StringDictionary.hpp"
#include <string>
#include <vector>

using System::Collections::Specialized::OrderedDictionary;
using System::Collections::Specialized::StringCollection;
using System::Collections::Specialized::StringDictionary;

// ===========================================================================
// OrderedDictionary
// ===========================================================================

TEST(OrderedDictionaryBatch22Test, DefaultCtor) {
    OrderedDictionary d;
    EXPECT_EQ(d.getCountProperty(), 0);
    EXPECT_FALSE(d.getIsReadOnlyProperty());
}

TEST(OrderedDictionaryBatch22Test, NullptrCtor) {
    OrderedDictionary d(nullptr);
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(OrderedDictionaryBatch22Test, IntCtor) {
    OrderedDictionary d(16);
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(OrderedDictionaryBatch22Test, IntNullptrCtor) {
    OrderedDictionary d(16, nullptr);
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(OrderedDictionaryBatch22Test, Properties) {
    OrderedDictionary d;
    EXPECT_FALSE(d.getIsFixedSizeProperty());
    EXPECT_FALSE(d.getIsSynchronizedProperty());
    EXPECT_NE(d.getSyncRootProperty(), nullptr);
}

TEST(OrderedDictionaryBatch22Test, AddAndContains) {
    OrderedDictionary d;
    d.Add("a", "1");
    d.Add("b", "2");
    EXPECT_EQ(d.getCountProperty(), 2);
    EXPECT_TRUE(d.Contains("a"));
    EXPECT_FALSE(d.Contains("c"));
}

TEST(OrderedDictionaryBatch22Test, Add_DuplicateThrows) {
    OrderedDictionary d;
    d.Add("k", "v");
    EXPECT_THROW(d.Add("k", "v2"), std::invalid_argument);
}

TEST(OrderedDictionaryBatch22Test, Indexer_ByKey) {
    OrderedDictionary d;
    d.Add("x", "hello");
    EXPECT_EQ(d["x"], "hello");
}

TEST(OrderedDictionaryBatch22Test, Indexer_Missing_Throws) {
    OrderedDictionary d;
    EXPECT_THROW((void)d["missing"], std::out_of_range);
}

TEST(OrderedDictionaryBatch22Test, GetByIndex) {
    OrderedDictionary d;
    d.Add("first", "A");
    d.Add("second", "B");
    EXPECT_EQ(d.GetByIndex(0), "A");
    EXPECT_EQ(d.GetByIndex(1), "B");
    EXPECT_EQ(d.GetKey(0), "first");
    EXPECT_EQ(d.GetKey(1), "second");
}

TEST(OrderedDictionaryBatch22Test, Insert_PreservesOrder) {
    OrderedDictionary d;
    d.Add("a", "1");
    d.Add("c", "3");
    d.Insert(1, "b", "2");
    EXPECT_EQ(d.GetKey(0), "a");
    EXPECT_EQ(d.GetKey(1), "b");
    EXPECT_EQ(d.GetKey(2), "c");
}

TEST(OrderedDictionaryBatch22Test, Remove_ByKey) {
    OrderedDictionary d;
    d.Add("a", "1");
    d.Add("b", "2");
    d.Remove("a");
    EXPECT_EQ(d.getCountProperty(), 1);
    EXPECT_FALSE(d.Contains("a"));
    EXPECT_EQ(d.GetKey(0), "b");
}

TEST(OrderedDictionaryBatch22Test, RemoveAt) {
    OrderedDictionary d;
    d.Add("x", "1");
    d.Add("y", "2");
    d.RemoveAt(0);
    EXPECT_EQ(d.getCountProperty(), 1);
    EXPECT_EQ(d.GetKey(0), "y");
}

TEST(OrderedDictionaryBatch22Test, Clear) {
    OrderedDictionary d;
    d.Add("a", "1");
    d.Clear();
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(OrderedDictionaryBatch22Test, KeysProperty_InsertionOrder) {
    OrderedDictionary d;
    d.Add("z", "1");
    d.Add("a", "2");
    d.Add("m", "3");
    const auto& keys = d.getKeysProperty();
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], "z");
    EXPECT_EQ(keys[1], "a");
    EXPECT_EQ(keys[2], "m");
}

TEST(OrderedDictionaryBatch22Test, ValuesProperty_InsertionOrder) {
    OrderedDictionary d;
    d.Add("a", "alpha");
    d.Add("b", "beta");
    const auto& vals = d.getValuesProperty();
    ASSERT_EQ(vals.size(), 2u);
    EXPECT_EQ(vals[0], "alpha");
    EXPECT_EQ(vals[1], "beta");
}

TEST(OrderedDictionaryBatch22Test, AsReadOnly_BlocksMutation) {
    OrderedDictionary d;
    d.Add("k", "v");
    d.AsReadOnly();
    EXPECT_TRUE(d.getIsReadOnlyProperty());
    EXPECT_THROW(d.Add("x", "y"), std::runtime_error);
    EXPECT_THROW(d.Remove("k"), std::runtime_error);
    EXPECT_THROW(d.Clear(), std::runtime_error);
}

// ===========================================================================
// StringCollection
// ===========================================================================

TEST(StringCollectionBatch22Test, IsSynchronized_SyncRoot) {
    StringCollection sc;
    EXPECT_FALSE(sc.getIsSynchronizedProperty());
    EXPECT_NE(sc.getSyncRootProperty(), nullptr);
}

TEST(StringCollectionBatch22Test, AddAndCount) {
    StringCollection sc;
    int idx = sc.Add("hello");
    EXPECT_EQ(idx, 0);
    sc.Add("world");
    EXPECT_EQ(sc.getCountProperty(), 2);
}

TEST(StringCollectionBatch22Test, Indexer_GetSet) {
    StringCollection sc;
    sc.Add("a");
    EXPECT_EQ(sc[0], "a");
    sc[0] = "b";
    EXPECT_EQ(sc[0], "b");
}

TEST(StringCollectionBatch22Test, Contains_IndexOf) {
    StringCollection sc;
    sc.Add("x"); sc.Add("y");
    EXPECT_TRUE(sc.Contains("x"));
    EXPECT_EQ(sc.IndexOf("y"), 1);
    EXPECT_EQ(sc.IndexOf("z"), -1);
}

TEST(StringCollectionBatch22Test, AddRange) {
    StringCollection sc;
    sc.AddRange({"a", "b", "c"});
    EXPECT_EQ(sc.getCountProperty(), 3);
}

TEST(StringCollectionBatch22Test, Remove_RemoveAt_Clear) {
    StringCollection sc;
    sc.Add("a"); sc.Add("b"); sc.Add("c");
    sc.Remove("b");
    EXPECT_EQ(sc.getCountProperty(), 2);
    sc.RemoveAt(0);
    EXPECT_EQ(sc[0], "c");
    sc.Clear();
    EXPECT_EQ(sc.getCountProperty(), 0);
}

TEST(StringCollectionBatch22Test, CopyTo) {
    StringCollection sc;
    sc.Add("p"); sc.Add("q");
    std::vector<std::string> dest(4, "");
    sc.CopyTo(dest, 1);
    EXPECT_EQ(dest[1], "p");
    EXPECT_EQ(dest[2], "q");
}

// ===========================================================================
// StringDictionary
// ===========================================================================

TEST(StringDictionaryBatch22Test, SyncRoot) {
    StringDictionary sd;
    EXPECT_NE(sd.getSyncRootProperty(), nullptr);
    EXPECT_FALSE(sd.getIsSynchronizedProperty());
}

TEST(StringDictionaryBatch22Test, AddAndCount) {
    StringDictionary sd;
    sd.Add("KEY", "val");
    EXPECT_EQ(sd.getCountProperty(), 1);
    EXPECT_TRUE(sd.ContainsKey("key")); // case-insensitive
}

TEST(StringDictionaryBatch22Test, GetKeysProperty) {
    StringDictionary sd;
    sd.Add("Alpha", "1");
    sd.Add("Beta", "2");
    auto keys = sd.getKeysProperty();
    EXPECT_EQ(keys.size(), 2u);
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "alpha") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "beta") != keys.end());
}

TEST(StringDictionaryBatch22Test, GetValuesProperty) {
    StringDictionary sd;
    sd.Add("k1", "v1");
    sd.Add("k2", "v2");
    auto vals = sd.getValuesProperty();
    EXPECT_EQ(vals.size(), 2u);
    EXPECT_TRUE(std::find(vals.begin(), vals.end(), "v1") != vals.end());
    EXPECT_TRUE(std::find(vals.begin(), vals.end(), "v2") != vals.end());
}

TEST(StringDictionaryBatch22Test, ContainsValue) {
    StringDictionary sd;
    sd.Add("k", "hello");
    EXPECT_TRUE(sd.ContainsValue("hello"));
    EXPECT_FALSE(sd.ContainsValue("world"));
}

TEST(StringDictionaryBatch22Test, GetValue_MissingReturnsEmpty) {
    StringDictionary sd;
    EXPECT_EQ(sd.GetValue("missing"), "");
    sd.Add("present", "found");
    EXPECT_EQ(sd.GetValue("PRESENT"), "found");
}

TEST(StringDictionaryBatch22Test, Remove_Clear) {
    StringDictionary sd;
    sd.Add("a", "1");
    sd.Add("b", "2");
    sd.Remove("A");
    EXPECT_EQ(sd.getCountProperty(), 1);
    sd.Clear();
    EXPECT_EQ(sd.getCountProperty(), 0);
}
