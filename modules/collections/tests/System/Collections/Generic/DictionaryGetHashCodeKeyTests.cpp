// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Collections/Generic/Dictionary.hpp"
#include "System/Collections/Generic/HashSet.hpp"
#include <string>
#include <unordered_map>

using namespace System::Collections::Generic;

namespace {

// A key type shaped the way .NET expects one: equality plus GetHashCode, and no
// std::hash specialization, because .NET's API never asks for one.
struct DotNetStyleKey {
    int X = 0;
    int Y = 0;

    bool operator==(const DotNetStyleKey& other) const {
        return X == other.X && Y == other.Y;
    }

    [[nodiscard]] int GetHashCode() const { return X * 397 ^ Y; }
};

// A key type with a std::hash specialization and no GetHashCode, which must keep using
// std::hash exactly as before.
struct StdHashOnlyKey {
    int Id = 0;
    bool operator==(const StdHashOnlyKey& other) const { return Id == other.Id; }
};

} // namespace

template<>
struct std::hash<StdHashOnlyKey> {
    std::size_t operator()(const StdHashOnlyKey& value) const noexcept {
        return std::hash<int>{}(value.Id);
    }
};

// .NET reaches EqualityComparer<TKey>.Default, which exists for every type; C++ reaches
// std::hash<TKey>, which does not. A key carrying the .NET contract is served from its
// own GetHashCode rather than being rejected.
TEST(DictionaryGetHashCodeKeyTests, AKeyWithOnlyGetHashCodeIsUsable) {
    Dictionary<DotNetStyleKey, int> map;

    map.Add(DotNetStyleKey{1, 2}, 12);
    map.Add(DotNetStyleKey{3, 4}, 34);

    EXPECT_EQ(map.getCountProperty(), 2);
    EXPECT_TRUE(map.ContainsKey(DotNetStyleKey{1, 2}));
    EXPECT_TRUE(map.ContainsKey(DotNetStyleKey{3, 4}));
    EXPECT_FALSE(map.ContainsKey(DotNetStyleKey{5, 6}));
    const DotNetStyleKey lookup{3, 4};
    EXPECT_EQ(map[lookup], 34);
}

TEST(DictionaryGetHashCodeKeyTests, EqualKeysAreTheSameEntry) {
    Dictionary<DotNetStyleKey, int> map;

    const DotNetStyleKey key{7, 8};
    map[key] = 1;
    map[key] = 2;

    EXPECT_EQ(map.getCountProperty(), 1);
    EXPECT_EQ(map[key], 2);
}

TEST(DictionaryGetHashCodeKeyTests, RemoveAndTryGetValueWorkForSuchAKey) {
    Dictionary<DotNetStyleKey, int> map;
    map.Add(DotNetStyleKey{1, 1}, 11);

    int value = 0;
    EXPECT_TRUE(map.TryGetValue(DotNetStyleKey{1, 1}, value));
    EXPECT_EQ(value, 11);

    EXPECT_TRUE(map.Remove(DotNetStyleKey{1, 1}));
    EXPECT_EQ(map.getCountProperty(), 0);
    EXPECT_FALSE(map.TryGetValue(DotNetStyleKey{1, 1}, value));
}

TEST(DictionaryGetHashCodeKeyTests, HashCollisionsStillSeparateUnequalKeys) {
    Dictionary<DotNetStyleKey, int> map;

    // 397*X ^ Y collides for these two, so both land in the same bucket and only
    // key equality can tell them apart.
    const DotNetStyleKey a{0, 397};
    const DotNetStyleKey b{1, 0};
    ASSERT_EQ(a.GetHashCode(), b.GetHashCode());
    ASSERT_FALSE(a == b);

    map.Add(a, 1);
    map.Add(b, 2);

    EXPECT_EQ(map.getCountProperty(), 2);
    EXPECT_EQ(map[a], 1);
    EXPECT_EQ(map[b], 2);
}

// The existing selector must be untouched for every key type that already worked.
TEST(DictionaryGetHashCodeKeyTests, AKeyWithStdHashStillUsesIt) {
    Dictionary<StdHashOnlyKey, int> map;
    map.Add(StdHashOnlyKey{42}, 1);

    EXPECT_TRUE(map.ContainsKey(StdHashOnlyKey{42}));
    EXPECT_EQ(map[StdHashOnlyKey{42}], 1);
    static_assert(std::is_same_v<Dictionary<StdHashOnlyKey, int>::MapType,
                                 std::unordered_map<StdHashOnlyKey, int,
                                                    std::hash<StdHashOnlyKey>,
                                                    std::equal_to<StdHashOnlyKey>>>,
                  "a key with std::hash must keep selecting it");
}

TEST(DictionaryGetHashCodeKeyTests, OrdinaryKeysAreUnaffected) {
    Dictionary<std::string, int> byString;
    byString.Add("one", 1);
    EXPECT_EQ(byString["one"], 1);

    Dictionary<int, std::string> byInt;
    byInt.Add(1, "one");
    const std::string fromInt = byInt[1];
    EXPECT_EQ(fromInt, "one");
}
