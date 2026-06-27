// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/Generic/HashSet.hpp"

using System::Collections::Generic::HashSet;

TEST(HashSetTest, DefaultEmpty) {
    HashSet<int> s;
    EXPECT_EQ(s.getCountProperty(), 0);
}

TEST(HashSetTest, AddAndContains) {
    HashSet<int> s;
    EXPECT_TRUE(s.Add(1));
    EXPECT_TRUE(s.Contains(1));
}

TEST(HashSetTest, AddDuplicateReturnsFalse) {
    HashSet<int> s;
    s.Add(5);
    EXPECT_FALSE(s.Add(5));
    EXPECT_EQ(s.getCountProperty(), 1);
}

TEST(HashSetTest, Remove) {
    HashSet<int> s;
    s.Add(3);
    EXPECT_TRUE(s.Remove(3));
    EXPECT_FALSE(s.Contains(3));
}

TEST(HashSetTest, RemoveMissingReturnsFalse) {
    HashSet<int> s;
    EXPECT_FALSE(s.Remove(99));
}

TEST(HashSetTest, Clear) {
    HashSet<int> s;
    s.Add(1); s.Add(2);
    s.Clear();
    EXPECT_EQ(s.getCountProperty(), 0);
}

TEST(HashSetTest, UnionWith) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2);
    b.Add(2); b.Add(3);
    a.UnionWith(b);
    EXPECT_EQ(a.getCountProperty(), 3);
    EXPECT_TRUE(a.Contains(3));
}

TEST(HashSetTest, IntersectWith) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2); a.Add(3);
    b.Add(2); b.Add(3); b.Add(4);
    a.IntersectWith(b);
    EXPECT_EQ(a.getCountProperty(), 2);
    EXPECT_FALSE(a.Contains(1));
    EXPECT_TRUE(a.Contains(2));
}

TEST(HashSetTest, ExceptWith) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2); a.Add(3);
    b.Add(2); b.Add(3);
    a.ExceptWith(b);
    EXPECT_EQ(a.getCountProperty(), 1);
    EXPECT_TRUE(a.Contains(1));
}

TEST(HashSetTest, SymmetricExceptWith) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2);
    b.Add(2); b.Add(3);
    a.SymmetricExceptWith(b);
    EXPECT_TRUE(a.Contains(1));
    EXPECT_FALSE(a.Contains(2));
    EXPECT_TRUE(a.Contains(3));
}

TEST(HashSetTest, SetEquals) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2);
    b.Add(2); b.Add(1);
    EXPECT_TRUE(a.SetEquals(b));
}

TEST(HashSetTest, IsSubsetOf) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2);
    b.Add(1); b.Add(2); b.Add(3);
    EXPECT_TRUE(a.IsSubsetOf(b));
    EXPECT_FALSE(b.IsSubsetOf(a));
}

TEST(HashSetTest, IsSupersetOf) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2); a.Add(3);
    b.Add(1); b.Add(2);
    EXPECT_TRUE(a.IsSupersetOf(b));
    EXPECT_FALSE(b.IsSupersetOf(a));
}

TEST(HashSetTest, IsProperSubsetOf) {
    HashSet<int> a, b;
    a.Add(1); a.Add(2);
    b.Add(1); b.Add(2); b.Add(3);
    EXPECT_TRUE(a.IsProperSubsetOf(b));
    EXPECT_FALSE(b.IsProperSubsetOf(a));
}

TEST(HashSetTest, ToArray) {
    HashSet<int> s;
    s.Add(1); s.Add(2);
    auto arr = s.ToArray();
    EXPECT_EQ(static_cast<int>(arr.size()), 2);
}

TEST(HashSetTest, RangeBasedFor) {
    HashSet<int> s;
    s.Add(10); s.Add(20); s.Add(30);
    int sum = 0;
    for (const auto& v : s) sum += v;
    EXPECT_EQ(sum, 60);
}
