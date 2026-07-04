// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "System/Collections/ObjectModel/Collection.hpp"
#include "System/Collections/ObjectModel/ReadOnlyCollection.hpp"
#include "System/Collections/ObjectModel/ReadOnlyDictionary.hpp"
#include "System/Collections/ObjectModel/ObservableCollection.hpp"
#include "System/NotSupportedException.hpp"

using System::Collections::ObjectModel::Collection;
using System::Collections::ObjectModel::ReadOnlyCollection;
using System::Collections::ObjectModel::ReadOnlyDictionary;
using System::Collections::ObjectModel::ObservableCollection;
using System::Collections::ObjectModel::NotifyCollectionChangedAction;
using System::Collections::ObjectModel::NotifyCollectionChangedEventArgs;

// ===========================================================================
// Collection<T>
// ===========================================================================

TEST(CollectionTests, DefaultCtor_IsEmpty) {
    Collection<int> c;
    EXPECT_EQ(c.getCountProperty(), 0);
    EXPECT_FALSE(c.getIsReadOnlyProperty());
}

TEST(CollectionTests, Add_IncreasesCount) {
    Collection<int> c;
    c.Add(10);
    c.Add(20);
    EXPECT_EQ(c.getCountProperty(), 2);
}

TEST(CollectionTests, Contains_True_False) {
    Collection<std::string> c;
    c.Add("hello");
    EXPECT_TRUE(c.Contains("hello"));
    EXPECT_FALSE(c.Contains("world"));
}

TEST(CollectionTests, IndexOf_Found) {
    Collection<int> c;
    c.Add(1);
    c.Add(2);
    c.Add(3);
    EXPECT_EQ(c.IndexOf(2), 1);
}

TEST(CollectionTests, IndexOf_NotFound_ReturnsMinusOne) {
    Collection<int> c;
    c.Add(1);
    EXPECT_EQ(c.IndexOf(99), -1);
}

TEST(CollectionTests, OperatorBracket_AccessByIndex) {
    Collection<int> c;
    c.Add(7);
    c.Add(8);
    EXPECT_EQ(c[0], 7);
    EXPECT_EQ(c[1], 8);
}

TEST(CollectionTests, Remove_RemovesFirstMatch) {
    Collection<int> c;
    c.Add(5);
    c.Add(10);
    EXPECT_TRUE(c.Remove(5));
    EXPECT_EQ(c.getCountProperty(), 1);
    EXPECT_EQ(c[0], 10);
}

TEST(CollectionTests, Remove_NotFound_ReturnsFalse) {
    Collection<int> c;
    c.Add(1);
    EXPECT_FALSE(c.Remove(99));
}

TEST(CollectionTests, Clear_EmptiesCollection) {
    Collection<int> c;
    c.Add(1);
    c.Add(2);
    c.Clear();
    EXPECT_EQ(c.getCountProperty(), 0);
}

TEST(CollectionTests, Insert_AtIndex) {
    Collection<int> c;
    c.Add(1);
    c.Add(3);
    c.Insert(1, 2);
    EXPECT_EQ(c.getCountProperty(), 3);
    EXPECT_EQ(c[1], 2);
}

TEST(CollectionTests, RemoveAt_RemovesByIndex) {
    Collection<int> c;
    c.Add(10);
    c.Add(20);
    c.Add(30);
    c.RemoveAt(1);
    EXPECT_EQ(c.getCountProperty(), 2);
    EXPECT_EQ(c[1], 30);
}

// ===========================================================================
// ReadOnlyCollection<T>
// ===========================================================================

TEST(ReadOnlyCollectionTests, Constructor_FromVector) {
    ReadOnlyCollection<int> rc(std::vector<int>{1, 2, 3});
    EXPECT_EQ(rc.getCountProperty(), 3);
    EXPECT_TRUE(rc.getIsReadOnlyProperty());
}

TEST(ReadOnlyCollectionTests, OperatorBracket_ReadAccess) {
    const ReadOnlyCollection<std::string> rc(std::vector<std::string>{"a", "b", "c"});
    EXPECT_EQ(rc[0], "a");
    EXPECT_EQ(rc[2], "c");
}

TEST(ReadOnlyCollectionTests, Contains_True_False) {
    ReadOnlyCollection<int> rc(std::vector<int>{10, 20, 30});
    EXPECT_TRUE(rc.Contains(20));
    EXPECT_FALSE(rc.Contains(99));
}

TEST(ReadOnlyCollectionTests, IndexOf_Found) {
    ReadOnlyCollection<int> rc(std::vector<int>{5, 10, 15});
    EXPECT_EQ(rc.IndexOf(10), 1);
    EXPECT_EQ(rc.IndexOf(99), -1);
}

TEST(ReadOnlyCollectionTests, Add_Throws) {
    ReadOnlyCollection<int> rc(std::vector<int>{1});
    EXPECT_THROW(rc.Add(2), System::NotSupportedException);
}

TEST(ReadOnlyCollectionTests, Clear_Throws) {
    ReadOnlyCollection<int> rc(std::vector<int>{1});
    EXPECT_THROW(rc.Clear(), System::NotSupportedException);
}

TEST(ReadOnlyCollectionTests, Remove_Throws) {
    ReadOnlyCollection<int> rc(std::vector<int>{1});
    EXPECT_THROW(rc.Remove(1), System::NotSupportedException);
}

TEST(ReadOnlyCollectionTests, DefaultCtor_CountZero) {
    ReadOnlyCollection<int> rc;
    EXPECT_EQ(rc.getCountProperty(), 0);
}

// ===========================================================================
// ReadOnlyDictionary<K,V>
// ===========================================================================

TEST(ReadOnlyDictionaryTests, Constructor_FromSharedMap) {
    auto m = std::make_shared<std::unordered_map<std::string, int>>();
    (*m)["a"] = 1;
    (*m)["b"] = 2;
    ReadOnlyDictionary<std::string, int> rd(m);
    EXPECT_EQ(rd.getCountProperty(), 2);
}

TEST(ReadOnlyDictionaryTests, ContainsKey_True_False) {
    auto m = std::make_shared<std::unordered_map<std::string, int>>();
    (*m)["x"] = 10;
    ReadOnlyDictionary<std::string, int> rd(m);
    EXPECT_TRUE(rd.ContainsKey("x"));
    EXPECT_FALSE(rd.ContainsKey("y"));
}

TEST(ReadOnlyDictionaryTests, OperatorBracket_ReturnsValue) {
    auto m = std::make_shared<std::unordered_map<std::string, int>>();
    (*m)["key"] = 42;
    ReadOnlyDictionary<std::string, int> rd(m);
    EXPECT_EQ(rd["key"], 42);
}

TEST(ReadOnlyDictionaryTests, TryGetValue_Found) {
    auto m = std::make_shared<std::unordered_map<std::string, int>>();
    (*m)["k"] = 99;
    ReadOnlyDictionary<std::string, int> rd(m);
    int v = 0;
    EXPECT_TRUE(rd.TryGetValue("k", v));
    EXPECT_EQ(v, 99);
}

TEST(ReadOnlyDictionaryTests, TryGetValue_NotFound_ReturnsFalse) {
    auto m = std::make_shared<std::unordered_map<std::string, int>>();
    ReadOnlyDictionary<std::string, int> rd(m);
    int v = 0;
    EXPECT_FALSE(rd.TryGetValue("missing", v));
}

TEST(ReadOnlyDictionaryTests, Keys_HasAllKeys) {
    auto m = std::make_shared<std::unordered_map<std::string, int>>();
    (*m)["a"] = 1;
    (*m)["b"] = 2;
    ReadOnlyDictionary<std::string, int> rd(m);
    auto keys = rd.getKeysProperty();
    EXPECT_EQ(keys.size(), 2u);
}

TEST(ReadOnlyDictionaryTests, Values_HasAllValues) {
    auto m = std::make_shared<std::unordered_map<std::string, int>>();
    (*m)["a"] = 10;
    ReadOnlyDictionary<std::string, int> rd(m);
    auto vals = rd.getValuesProperty();
    EXPECT_EQ(vals.size(), 1u);
    EXPECT_EQ(vals[0], 10);
}

// ===========================================================================
// NotifyCollectionChangedAction enum
// ===========================================================================

TEST(NotifyCollectionChangedActionTests, Add_IsZero) {
    EXPECT_EQ(static_cast<int>(NotifyCollectionChangedAction::Add), 0);
}

TEST(NotifyCollectionChangedActionTests, Remove_IsOne) {
    EXPECT_EQ(static_cast<int>(NotifyCollectionChangedAction::Remove), 1);
}

TEST(NotifyCollectionChangedActionTests, Reset_IsFour) {
    EXPECT_EQ(static_cast<int>(NotifyCollectionChangedAction::Reset), 4);
}

// ===========================================================================
// ObservableCollection<T>
// ===========================================================================

TEST(ObservableCollectionTests, DefaultCtor_IsEmpty) {
    ObservableCollection<int> oc;
    EXPECT_EQ(oc.getCountProperty(), 0);
}

TEST(ObservableCollectionTests, VectorCtor_InitializesItems) {
    ObservableCollection<int> oc(std::vector<int>{1, 2, 3});
    EXPECT_EQ(oc.getCountProperty(), 3);
}

TEST(ObservableCollectionTests, Add_TriggersCollectionChanged) {
    ObservableCollection<int> oc;
    int eventCount = 0;
    NotifyCollectionChangedAction lastAction = NotifyCollectionChangedAction::Reset;
    oc.CollectionChanged.push_back([&](void*, const NotifyCollectionChangedEventArgs<int>& args) {
        ++eventCount;
        lastAction = args.Action;
    });
    oc.Add(42);
    EXPECT_EQ(eventCount, 1);
    EXPECT_EQ(lastAction, NotifyCollectionChangedAction::Add);
}

TEST(ObservableCollectionTests, Remove_TriggersCollectionChanged) {
    ObservableCollection<int> oc(std::vector<int>{1, 2, 3});
    NotifyCollectionChangedAction lastAction = NotifyCollectionChangedAction::Add;
    oc.CollectionChanged.push_back([&](void*, const NotifyCollectionChangedEventArgs<int>& args) {
        lastAction = args.Action;
    });
    oc.Remove(2);
    EXPECT_EQ(lastAction, NotifyCollectionChangedAction::Remove);
    EXPECT_EQ(oc.getCountProperty(), 2);
}

TEST(ObservableCollectionTests, Clear_TriggersResetEvent) {
    ObservableCollection<int> oc(std::vector<int>{1, 2});
    NotifyCollectionChangedAction lastAction = NotifyCollectionChangedAction::Add;
    oc.CollectionChanged.push_back([&](void*, const NotifyCollectionChangedEventArgs<int>& args) {
        lastAction = args.Action;
    });
    oc.Clear();
    EXPECT_EQ(lastAction, NotifyCollectionChangedAction::Reset);
    EXPECT_EQ(oc.getCountProperty(), 0);
}

TEST(ObservableCollectionTests, NoHandlers_AddDoesNotThrow) {
    ObservableCollection<int> oc;
    EXPECT_NO_THROW(oc.Add(1));
}
