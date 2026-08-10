// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #1925: direct nullable-floating collection-key contract.
#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/Collections/Concurrent/ConcurrentDictionary.hpp"
#include "System/Collections/Frozen/FrozenDictionary.hpp"
#include "System/Collections/Frozen/FrozenSet.hpp"
#include "System/Collections/Generic/Dictionary.hpp"
#include "System/Collections/Generic/HashSet.hpp"
#include "System/Collections/Generic/OrderedDictionary.hpp"
#include "System/Collections/Generic/SortedDictionary.hpp"
#include "System/Collections/Generic/SortedList.hpp"
#include "System/Collections/Generic/SortedSet.hpp"
#include "System/Collections/Immutable/ImmutableDictionary.hpp"
#include "System/Collections/Immutable/ImmutableHashSet.hpp"
#include "System/Collections/Immutable/ImmutableSortedDictionary.hpp"
#include "System/Collections/Immutable/ImmutableSortedSet.hpp"
#include "System/Collections/ObjectModel/KeyedCollection.hpp"
#include "System/Collections/ObjectModel/ReadOnlyDictionary.hpp"
#include "System/Collections/ObjectModel/ReadOnlySet.hpp"
#include "System/detail/ComparisonPolicy.hpp"

namespace NullableFloatingCollectionKeyContractSupport {

namespace C = System::Collections::Concurrent;
namespace Fz = System::Collections::Frozen;
namespace G = System::Collections::Generic;
namespace I = System::Collections::Immutable;
namespace OM = System::Collections::ObjectModel;
namespace P = System::detail;

template<typename F>
F payloadNaN();

template<>
float payloadNaN<float>() {
    return std::bit_cast<float>(UINT32_C(0x7fc00007));
}

template<>
double payloadNaN<double>() {
    return std::bit_cast<double>(UINT64_C(0x7ff8000000000042));
}

template<>
long double payloadNaN<long double>() {
    return std::nanl("1925");
}

template<typename F>
struct KeyedItem {
    std::optional<F> key;
    int value;
    friend bool operator==(const KeyedItem&, const KeyedItem&) = default;
};

template<typename F>
class NullableKeyedCollection final
    : public OM::KeyedCollection<std::optional<F>, KeyedItem<F>> {
protected:
    std::optional<F> GetKeyForItem(const KeyedItem<F>& item) const override {
        return item.key;
    }
};

template<typename F>
void mutableHashedAndProjectionMatrix() {
    using O = std::optional<F>;
    const O none;
    const O nan(std::numeric_limits<F>::quiet_NaN());
    const O otherNaN(payloadNaN<F>());
    const O zero(F(0));
    const O negativeZero(-F(0));
    const O positiveInfinity(std::numeric_limits<F>::infinity());
    const O negativeInfinity(-std::numeric_limits<F>::infinity());

    G::Dictionary<O, int> dictionary;
    EXPECT_TRUE(dictionary.TryAdd(none, 1));
    EXPECT_FALSE(dictionary.TryAdd(none, 2));
    EXPECT_TRUE(dictionary.TryAdd(nan, 3));
    EXPECT_FALSE(dictionary.TryAdd(otherNaN, 4));
    EXPECT_TRUE(dictionary.TryAdd(zero, 5));
    EXPECT_FALSE(dictionary.TryAdd(negativeZero, 6));
    dictionary.Add(positiveInfinity, 7);
    dictionary.Add(negativeInfinity, 8);
    EXPECT_EQ(dictionary.getCountProperty(), 5);
    int value = 0;
    EXPECT_TRUE(dictionary.TryGetValue(none, value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(dictionary.TryGetValue(otherNaN, value));
    EXPECT_EQ(value, 3);
    for (int i = 0; i < 4096; ++i)
        dictionary.TryAdd(O(static_cast<F>(i) + F(0.25)), i);
    EXPECT_TRUE(dictionary.TryGetValue(otherNaN, value));
    dictionary.TrimExcess();
    EXPECT_TRUE(dictionary.TryGetValue(otherNaN, value));
    EXPECT_TRUE(dictionary.Remove(otherNaN));
    EXPECT_TRUE(dictionary.TryAdd(nan, 9));
    G::Dictionary<O, int> dictionaryCopy(dictionary);
    G::Dictionary<O, int> dictionaryMoved(std::move(dictionaryCopy));
    EXPECT_TRUE(dictionaryMoved.ContainsKey(otherNaN));
    std::size_t dictionaryVisited = 0;
    for (const auto& entry : dictionaryMoved) {
        (void)entry;
        ++dictionaryVisited;
    }
    EXPECT_EQ(dictionaryVisited,
              static_cast<std::size_t>(dictionaryMoved.getCountProperty()));

    G::HashSet<O> hashSet;
    EXPECT_TRUE(hashSet.Add(none));
    EXPECT_FALSE(hashSet.Add(none));
    EXPECT_TRUE(hashSet.Add(nan));
    EXPECT_FALSE(hashSet.Add(otherNaN));
    EXPECT_TRUE(hashSet.Add(zero));
    EXPECT_FALSE(hashSet.Add(negativeZero));
    hashSet.Add(positiveInfinity);
    hashSet.Add(negativeInfinity);
    for (int i = 0; i < 4096; ++i)
        hashSet.Add(O(static_cast<F>(i) + F(0.25)));
    EXPECT_TRUE(hashSet.Contains(otherNaN));
    hashSet.TrimExcess();
    EXPECT_TRUE(hashSet.Contains(otherNaN));
    G::HashSet<O> hashSetCopy(hashSet);
    G::HashSet<O> hashSetMoved(std::move(hashSetCopy));
    EXPECT_TRUE(hashSetMoved.Contains(otherNaN));

    G::HashSet<O> left;
    left.Add(none); left.Add(nan); left.Add(O(F(1))); left.Add(O(F(2)));
    G::HashSet<O> right;
    right.Add(none); right.Add(otherNaN); right.Add(O(F(2))); right.Add(O(F(3)));
    G::HashSet<O> united(left); united.UnionWith(right);
    EXPECT_EQ(united.getCountProperty(), 5);
    G::HashSet<O> intersected(left); intersected.IntersectWith(right);
    EXPECT_EQ(intersected.getCountProperty(), 3);
    EXPECT_TRUE(intersected.Contains(none));
    EXPECT_TRUE(intersected.Contains(otherNaN));
    EXPECT_TRUE(left.SetEquals(left));

    auto frozenSet = Fz::FrozenSet<O>::Create(
        std::vector<O>{none, none, nan, otherNaN, zero, negativeZero, O(F(1))});
    EXPECT_EQ(frozenSet.getCountProperty(), 4);
    EXPECT_TRUE(frozenSet.Contains(none));
    EXPECT_TRUE(frozenSet.Contains(otherNaN));
    auto frozenDictionary = Fz::FrozenDictionary<O, int>::Create(
        std::vector<std::pair<O, int>>{{none, 1}, {none, 2}, {nan, 3},
                                       {otherNaN, 4}, {zero, 5}, {negativeZero, 6}});
    EXPECT_EQ(frozenDictionary.getCountProperty(), 3);
    EXPECT_TRUE(frozenDictionary.TryGetValue(none, value));
    EXPECT_EQ(value, 2);
    EXPECT_TRUE(frozenDictionary.TryGetValue(otherNaN, value));
    EXPECT_EQ(value, 4);

    auto readOnlySetBacking = std::make_shared<typename OM::ReadOnlySet<O>::SetType>();
    readOnlySetBacking->insert(none);
    readOnlySetBacking->insert(nan);
    readOnlySetBacking->insert(otherNaN);
    OM::ReadOnlySet<O> readOnlySet(readOnlySetBacking);
    EXPECT_EQ(readOnlySet.getCountProperty(), 2);
    EXPECT_TRUE(readOnlySet.Contains(none));
    EXPECT_TRUE(readOnlySet.Contains(otherNaN));
    EXPECT_TRUE(readOnlySet.SetEquals(readOnlySet));
    auto readOnlyMapBacking =
        std::make_shared<typename OM::ReadOnlyDictionary<O, int>::MapType>();
    (*readOnlyMapBacking)[none] = 1;
    (*readOnlyMapBacking)[nan] = 2;
    (*readOnlyMapBacking)[otherNaN] = 3;
    OM::ReadOnlyDictionary<O, int> readOnlyDictionary(readOnlyMapBacking);
    EXPECT_EQ(readOnlyDictionary.getCountProperty(), 2);
    EXPECT_TRUE(readOnlyDictionary.TryGetValue(none, value));
    EXPECT_TRUE(readOnlyDictionary.TryGetValue(otherNaN, value));
    EXPECT_EQ(value, 3);

    auto immutableDictionary = I::ImmutableDictionary<O, int>::Empty()
        .Add(none, 1).Add(nan, 2).Add(otherNaN, 2).Add(zero, 3);
    EXPECT_EQ(immutableDictionary.getCountProperty(), 3);
    EXPECT_TRUE(immutableDictionary.ContainsKey(otherNaN));
    EXPECT_EQ(immutableDictionary.Remove(otherNaN).getCountProperty(), 2);
    auto immutableHashSet = I::ImmutableHashSet<O>::Empty()
        .Add(none).Add(none).Add(nan).Add(otherNaN).Add(zero).Add(negativeZero);
    EXPECT_EQ(immutableHashSet.getCountProperty(), 3);
    EXPECT_TRUE(immutableHashSet.Contains(otherNaN));
    EXPECT_TRUE(immutableHashSet.SetEquals(immutableHashSet));

    C::ConcurrentDictionary<O, int> concurrentDictionary;
    EXPECT_TRUE(concurrentDictionary.TryAdd(none, 1));
    EXPECT_FALSE(concurrentDictionary.TryAdd(none, 2));
    EXPECT_TRUE(concurrentDictionary.TryAdd(nan, 3));
    EXPECT_FALSE(concurrentDictionary.TryAdd(otherNaN, 4));
    EXPECT_TRUE(concurrentDictionary.TryGetValue(otherNaN, value));
    EXPECT_EQ(value, 3);
    EXPECT_TRUE(concurrentDictionary.TryRemove(otherNaN, value));

    G::OrderedDictionary<O, int> orderedDictionary;
    orderedDictionary.Add(none, 1);
    orderedDictionary.Add(nan, 2);
    EXPECT_THROW(orderedDictionary.Add(otherNaN, 3), System::ArgumentException);
    orderedDictionary.Add(zero, 4);
    EXPECT_THROW(orderedDictionary.Add(negativeZero, 5), System::ArgumentException);
    EXPECT_TRUE(orderedDictionary.TryGetValue(otherNaN, value));
    for (int i = 0; i < 512; ++i)
        orderedDictionary.TryAdd(O(static_cast<F>(i) + F(0.25)), i);
    EXPECT_TRUE(orderedDictionary.ContainsKey(otherNaN));
    EXPECT_TRUE(orderedDictionary.Remove(otherNaN));

    NullableKeyedCollection<F> keyed;
    keyed.Add(KeyedItem<F>{none, 1});
    keyed.Add(KeyedItem<F>{nan, 2});
    EXPECT_THROW(keyed.Add(KeyedItem<F>{otherNaN, 3}), System::ArgumentException);
    EXPECT_TRUE(keyed.Contains(none));
    EXPECT_TRUE(keyed.Contains(otherNaN));
    KeyedItem<F> keyedValue{};
    EXPECT_TRUE(keyed.TryGetValue(otherNaN, keyedValue));
    EXPECT_EQ(keyedValue.value, 2);
    EXPECT_TRUE(keyed.Remove(otherNaN));
}

template<typename F>
void orderedMatrix() {
    using O = std::optional<F>;
    const O none;
    const O nan(std::numeric_limits<F>::quiet_NaN());
    const O otherNaN(payloadNaN<F>());
    const O positiveInfinity(std::numeric_limits<F>::infinity());
    const O negativeInfinity(-std::numeric_limits<F>::infinity());
    const O lowest(std::numeric_limits<F>::lowest());
    const O maximum(std::numeric_limits<F>::max());
    const O zero(F(0));
    const O negativeZero(-F(0));

    G::SortedSet<O> sortedSet;
    EXPECT_TRUE(sortedSet.Add(nan));
    EXPECT_TRUE(sortedSet.Add(O(F(1))));
    EXPECT_TRUE(sortedSet.Add(O(F(2))));
    EXPECT_TRUE(sortedSet.Add(none));
    EXPECT_FALSE(sortedSet.Add(otherNaN));
    EXPECT_TRUE(sortedSet.Add(negativeInfinity));
    EXPECT_TRUE(sortedSet.Add(positiveInfinity));
    EXPECT_TRUE(sortedSet.Add(lowest));
    EXPECT_TRUE(sortedSet.Add(maximum));
    EXPECT_TRUE(sortedSet.Add(zero));
    EXPECT_FALSE(sortedSet.Add(negativeZero));
    EXPECT_TRUE(sortedSet.Contains(otherNaN));
    const auto ordered = sortedSet.ToVector();
    EXPECT_GE(ordered.size(), 4u);
    if (ordered.size() >= 3u) {
        EXPECT_FALSE(ordered[0].has_value());
        EXPECT_TRUE(std::isnan(*ordered[1]));
        EXPECT_EQ(*ordered[2], -std::numeric_limits<F>::infinity());
    }
    G::SortedSet<O> sortedCopy(sortedSet);
    G::SortedSet<O> sortedMoved(std::move(sortedCopy));
    EXPECT_TRUE(sortedMoved.Contains(otherNaN));

    G::SortedSet<O> left{none, nan, O(F(1)), O(F(2))};
    G::SortedSet<O> right{none, otherNaN, O(F(2)), O(F(3))};
    G::SortedSet<O> united(left); united.UnionWith(right);
    EXPECT_EQ(united.getCountProperty(), 5);
    G::SortedSet<O> intersected(left); intersected.IntersectWith(right);
    EXPECT_EQ(intersected.getCountProperty(), 3);
    EXPECT_TRUE(intersected.Contains(otherNaN));
    EXPECT_TRUE(left.SetEquals(left));
    auto view = sortedSet.GetViewBetween(none, O(F(2)));
    EXPECT_TRUE(view.Contains(none));
    EXPECT_TRUE(view.Contains(otherNaN));

    G::SortedDictionary<O, int> sortedDictionary;
    sortedDictionary.Add(O(F(1)), 1);
    EXPECT_NO_THROW(sortedDictionary.Add(nan, 2));
    sortedDictionary.Add(none, 3);
    EXPECT_THROW(sortedDictionary.Add(otherNaN, 4), System::ArgumentException);
    int value = 0;
    EXPECT_TRUE(sortedDictionary.TryGetValue(otherNaN, value));
    EXPECT_EQ(value, 2);
    std::vector<O> dictionaryKeys;
    for (const auto& item : sortedDictionary) dictionaryKeys.push_back(item.first);
    EXPECT_EQ(dictionaryKeys.size(), 3u);
    if (dictionaryKeys.size() >= 2u) {
        EXPECT_FALSE(dictionaryKeys[0].has_value());
        EXPECT_TRUE(std::isnan(*dictionaryKeys[1]));
    }

    G::SortedList<O, int> sortedList;
    sortedList.Add(O(F(1)), 1);
    EXPECT_NO_THROW(sortedList.Add(nan, 2));
    sortedList.Add(none, 3);
    EXPECT_THROW(sortedList.Add(otherNaN, 4), System::ArgumentException);
    EXPECT_EQ(sortedList.IndexOfKey(none), 0);
    EXPECT_EQ(sortedList.IndexOfKey(otherNaN), 1);
    EXPECT_TRUE(sortedList.TryGetValue(otherNaN, value));

    auto immutableSortedDictionary =
        I::ImmutableSortedDictionary<O, int>::Empty().Add(O(F(1)), 1);
    EXPECT_NO_THROW(immutableSortedDictionary = immutableSortedDictionary.Add(nan, 2));
    immutableSortedDictionary = immutableSortedDictionary.Add(none, 3);
    immutableSortedDictionary = immutableSortedDictionary.SetItem(otherNaN, 4);
    EXPECT_EQ(immutableSortedDictionary.getCountProperty(), 3);
    EXPECT_TRUE(immutableSortedDictionary.TryGetValue(otherNaN, value));
    EXPECT_EQ(value, 4);
    const auto immutableKeys = immutableSortedDictionary.getKeysProperty();
    EXPECT_EQ(immutableKeys.size(), 3u);
    if (immutableKeys.size() >= 2u) {
        EXPECT_FALSE(immutableKeys[0].has_value());
        EXPECT_TRUE(std::isnan(*immutableKeys[1]));
    }

    auto immutableSortedSet = I::ImmutableSortedSet<O>::Create(
        std::vector<O>{O(F(1)), O(F(2)), nan, otherNaN, none, zero, negativeZero});
    EXPECT_EQ(immutableSortedSet.getCountProperty(), 5);
    EXPECT_TRUE(immutableSortedSet.Contains(otherNaN));
    EXPECT_TRUE(immutableSortedSet.SetEquals(immutableSortedSet));
    EXPECT_FALSE(immutableSortedSet.getMinProperty().has_value());
}

template<typename F>
void runCompleteMatrix() {
    mutableHashedAndProjectionMatrix<F>();
    orderedMatrix<F>();
}

} // namespace NullableFloatingCollectionKeyContractSupport

using namespace NullableFloatingCollectionKeyContractSupport;

TEST(NullableFloatingCollectionKeyContract, FloatCompleteSixteenConsumerMatrix) {
    runCompleteMatrix<float>();
}

TEST(NullableFloatingCollectionKeyContract, DoubleCompleteSixteenConsumerMatrix) {
    runCompleteMatrix<double>();
}

TEST(NullableFloatingCollectionKeyContract, LongDoubleCompleteSixteenConsumerMatrix) {
    runCompleteMatrix<long double>();
}

TEST(NullableFloatingCollectionKeyContract, AliasBoundaryIsExactAndNonFloatingIsTokenIdentical) {
    using OF = std::optional<float>;
    using OD = std::optional<double>;
    using OL = std::optional<long double>;
#if !defined(SHARP_RUNTIME_PRE_1925_PROBE) && \
    !defined(SHARP_RUNTIME_1925_ABI_PROBE)
    static_assert(std::is_same_v<P::DefaultKeyLess<OF>, P::DefaultLess<OF>>);
    static_assert(std::is_same_v<P::DefaultKeyHash<OF>, P::DefaultHash<OF>>);
    static_assert(std::is_same_v<P::DefaultKeyEqual<OF>, P::DefaultEqualTo<OF>>);
    static_assert(std::is_same_v<P::DefaultKeyLess<OD>, P::DefaultLess<OD>>);
    static_assert(std::is_same_v<P::DefaultKeyHash<OD>, P::DefaultHash<OD>>);
    static_assert(std::is_same_v<P::DefaultKeyEqual<OD>, P::DefaultEqualTo<OD>>);
    static_assert(std::is_same_v<P::DefaultKeyLess<OL>, P::DefaultLess<OL>>);
    static_assert(std::is_same_v<P::DefaultKeyHash<OL>, P::DefaultHash<OL>>);
    static_assert(std::is_same_v<P::DefaultKeyEqual<OL>, P::DefaultEqualTo<OL>>);

    static_assert(std::is_same_v<P::DefaultKeyLess<std::optional<int>>,
                                 std::less<std::optional<int>>>);
    static_assert(std::is_same_v<P::DefaultKeyHash<std::optional<int>>,
                                 std::hash<std::optional<int>>>);
    static_assert(std::is_same_v<P::DefaultKeyEqual<std::optional<int>>,
                                 std::equal_to<std::optional<int>>>);
    static_assert(std::is_same_v<P::DefaultKeyLess<std::optional<std::optional<double>>>,
                                 std::less<std::optional<std::optional<double>>>>);
    static_assert(std::is_same_v<P::DefaultKeyLess<std::pair<int, double>>,
                                 std::less<std::pair<int, double>>>);
    static_assert(std::is_same_v<P::DefaultKeyHash<std::tuple<double>>,
                                 std::hash<std::tuple<double>>>);

    static_assert(std::is_same_v<typename G::Dictionary<OD, int>::MapType,
        std::unordered_map<OD, int, P::DefaultHash<OD>, P::DefaultEqualTo<OD>>>);
    static_assert(std::is_same_v<typename G::HashSet<OD>::SetType,
        std::unordered_set<OD, P::DefaultHash<OD>, P::DefaultEqualTo<OD>>>);
    static_assert(std::is_same_v<typename Fz::FrozenDictionary<OD, int>::MapType,
        typename G::Dictionary<OD, int>::MapType>);
    static_assert(std::is_same_v<typename Fz::FrozenSet<OD>::SetType,
        typename G::HashSet<OD>::SetType>);
    static_assert(std::is_same_v<typename OM::ReadOnlyDictionary<OD, int>::MapType,
        typename G::Dictionary<OD, int>::MapType>);
    static_assert(std::is_same_v<typename OM::ReadOnlySet<OD>::SetType,
        typename G::HashSet<OD>::SetType>);
#elif defined(SHARP_RUNTIME_PRE_1925_PROBE)
    static_assert(std::is_same_v<P::DefaultKeyLess<OD>, std::less<OD>>);
    static_assert(std::is_same_v<P::DefaultKeyHash<OD>, std::hash<OD>>);
    static_assert(std::is_same_v<P::DefaultKeyEqual<OD>, std::equal_to<OD>>);
#endif
    static_assert(std::is_same_v<typename G::Dictionary<int, int>::MapType,
                                 std::unordered_map<int, int>>);
    static_assert(std::is_same_v<typename G::HashSet<std::string>::SetType,
                                 std::unordered_set<std::string>>);
    SUCCEED();
}

TEST(NullableFloatingCollectionKeyContract, ExplicitRuntimeComparersRemainAuthoritative) {
    using O = std::optional<double>;
    const O nan(std::numeric_limits<double>::quiet_NaN());
    auto rawHash = [](const O& value) { return std::hash<O>{}(value); };
    auto rawEqual = [](const O& left, const O& right) { return left == right; };
    auto rawSet = I::ImmutableHashSet<O>::Create(rawHash, rawEqual)
        .Add(nan).Add(nan);
    EXPECT_EQ(rawSet.getCountProperty(), 2);
    EXPECT_FALSE(rawSet.Contains(nan));

    auto allEquivalent = I::ImmutableSortedSet<O>::Create(
        [](const O&, const O&) { return false; },
        std::vector<O>{O(1.0), O(2.0), nan});
    EXPECT_EQ(allEquivalent.getCountProperty(), 1);
}
