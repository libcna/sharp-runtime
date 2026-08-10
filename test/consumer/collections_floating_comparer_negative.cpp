// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1919 (family #1912, CCF-010 Collections
// continuation). It pins the EXACT migration boundary the approved public type
// change draws.
//
// #1919 gave SortedSet, Dictionary, HashSet, FrozenSet, FrozenDictionary,
// ReadOnlySet and ReadOnlyDictionary the .NET default comparison contract by
// changing the comparator / hasher / equality template argument of their
// backing std:: container. For every NON-floating element or key type the
// alias is token-identical to the standard default, so nothing at all changes.
// For a `float`, `double` or `long double` element or key the backing type
// genuinely moves, and every public surface that names it moves with it.
//
// A whole-file "does it fail to compile" check cannot express that boundary:
// one broken line would hide every other line, and the interesting claim here
// is that HALF of these spellings must be rejected while the other half must
// still be accepted. Each site below is therefore compiled on its own by
// scripts/check_negative_consumer_fixtures.py, and the #else branches -- which
// are what the baseline compiles -- are the MIGRATED spellings that must keep
// working. The record is docs/NegativeConsumerFixtureValidation.md and the
// migration guide is docs/Migration-CollectionsFloatingComparers.md.
//
// Migration for a caller that hits any of these: spell the class's own alias.
// Dictionary<K,V>::MapType, HashSet<T>::SetType, FrozenSet<T>::SetType,
// FrozenDictionary<K,V>::MapType, ReadOnlySet<T>::SetType and
// ReadOnlyDictionary<K,V>::MapType are correct for EVERY element or key type,
// floating or not, so the migrated spelling needs no conditional.
//
// NEGATIVE-FIXTURE: component=Collections.Core
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "System/Collections/Frozen/FrozenDictionary.hpp"
#include "System/Collections/Frozen/FrozenSet.hpp"
#include "System/Collections/Generic/Dictionary.hpp"
#include "System/Collections/Generic/HashSet.hpp"
#include "System/Collections/ObjectModel/ReadOnlyDictionary.hpp"
#include "System/Collections/ObjectModel/ReadOnlySet.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Collections::Frozen::FrozenDictionary;
using System::Collections::Frozen::FrozenSet;
using System::Collections::Generic::Dictionary;
using System::Collections::Generic::HashSet;  // used by the non-floating baseline assertions
using System::Collections::ObjectModel::ReadOnlyDictionary;
using System::Collections::ObjectModel::ReadOnlySet;

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(readonlyset-double-raw-unordered-set): no matching function for call
    //     | no known conversion
    //     | could not convert
    auto raw = std::make_shared<std::unordered_set<double>>();
    ReadOnlySet<double> ros(raw);
    (void)ros;
#else
    auto migrated1 = std::make_shared<ReadOnlySet<double>::SetType>();
    ReadOnlySet<double> ros(migrated1);
    (void)ros;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(readonlydict-double-raw-unordered-map): no matching function for call
    //     | no known conversion
    //     | could not convert
    auto raw = std::make_shared<std::unordered_map<double, int>>();
    ReadOnlyDictionary<double, int> rod(raw);
    (void)rod;
#else
    auto migrated2 = std::make_shared<ReadOnlyDictionary<double, int>::MapType>();
    ReadOnlyDictionary<double, int> rod(migrated2);
    (void)rod;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(frozenset-double-createfromset-raw): cannot convert
    //     | no matching function for call
    //     | could not convert
    //     | no known conversion
    std::unordered_set<double> raw;
    (void)FrozenSet<double>::CreateFromSet(raw);
#else
    FrozenSet<double>::SetType migrated3;
    (void)FrozenSet<double>::CreateFromSet(migrated3);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(frozendict-double-createfrommap-raw): cannot convert
    //     | no matching function for call
    //     | could not convert
    //     | no known conversion
    std::unordered_map<double, int> raw;
    (void)FrozenDictionary<double, int>::CreateFromMap(raw);
#else
    FrozenDictionary<double, int>::MapType migrated4;
    (void)FrozenDictionary<double, int>::CreateFromMap(migrated4);
#endif

    Dictionary<double, int> floatingDict;
#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(dictionary-double-tomap-raw-reference): invalid initialization of reference
    //     | binding reference of type
    //     | cannot bind
    //     | invalid conversion
    const std::unordered_map<double, int>& backing = floatingDict.ToMap();
    (void)backing;
#else
    const Dictionary<double, int>::MapType& backing = floatingDict.ToMap();
    (void)backing;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // The floating instantiation genuinely MOVED. If this static_assert ever
    // passes, the repair has been silently reverted and every NaN key is
    // unfindable again.
    // NEGATIVE(dictionary-double-maptype-must-not-be-raw): static assertion failed
    static_assert(std::is_same_v<Dictionary<double, int>::MapType,
                                 std::unordered_map<double, int>>,
                  "site 6 must be rejected: the floating map type must have moved");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 7
    // The NON-floating instantiation did NOT move -- this is the other half of
    // the boundary, and it is what makes the change safe for every consumer
    // that does not key on a floating-point type. If this static_assert ever
    // passes, a non-floating consumer has been broken.
    // NEGATIVE(dictionary-int-maptype-must-stay-raw): static assertion failed
    static_assert(!std::is_same_v<Dictionary<int, int>::MapType,
                                  std::unordered_map<int, int>>,
                  "site 7 must be rejected: the non-floating map type must NOT have moved");
#endif

    // MEASURED CORRECTION to docs/CollectionsComparisonContractPlan.md section
    // 10, which said the public iterator typedefs of Dictionary<double,V>,
    // HashSet<double>, FrozenSet<double> and FrozenDictionary<double,V> change.
    // They do NOT: libstdc++'s node iterator is parameterised on the value type
    // and two bools, none of which mentions the hasher, and both bools are
    // unchanged for float and double. Only `long double` moves, because
    // __is_fast_hash<std::hash<long double>> is false while it is true for the
    // policy hasher, so the node stops caching its hash code. These two
    // baseline assertions pin the measurement in both directions.
    static_assert(std::is_same_v<FrozenSet<double>::const_iterator,
                                 std::unordered_set<double>::const_iterator>,
                  "measured: a double node iterator does NOT move");
    static_assert(!std::is_same_v<FrozenSet<long double>::const_iterator,
                                  std::unordered_set<long double>::const_iterator>,
                  "measured: a long double node iterator DOES move");

#if SHARP_RUNTIME_NEGATIVE_SITE == 8
    // NEGATIVE(frozenset-longdouble-iterator-raw): cannot convert
    //     | conversion from
    //     | no viable conversion
    //     | invalid conversion
    //     | no matching function for call
    FrozenSet<long double>::SetType raw;
    auto fs = FrozenSet<long double>::CreateFromSet(raw);
    std::unordered_set<long double>::const_iterator it = fs.begin();
    (void)it;
#else
    FrozenSet<long double>::SetType migrated8;
    auto fs = FrozenSet<long double>::CreateFromSet(migrated8);
    FrozenSet<long double>::const_iterator it = fs.begin();
    (void)it;
#endif

    using NullableDouble = std::optional<double>;
    using NullableLongDouble = std::optional<long double>;

#if SHARP_RUNTIME_NEGATIVE_SITE == 9
    // NEGATIVE(readonlyset-nullable-double-raw-unordered-set): no matching function for call
    //     | no known conversion
    auto rawNullableSet = std::make_shared<std::unordered_set<NullableDouble>>();
    ReadOnlySet<NullableDouble> nullableRos(rawNullableSet);
    (void)nullableRos;
#else
    auto migrated9 = std::make_shared<ReadOnlySet<NullableDouble>::SetType>();
    ReadOnlySet<NullableDouble> nullableRos(migrated9);
    (void)nullableRos;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 10
    // NEGATIVE(readonlydict-nullable-double-raw-unordered-map): no matching function for call
    //     | no known conversion
    auto rawNullableMap = std::make_shared<std::unordered_map<NullableDouble, int>>();
    ReadOnlyDictionary<NullableDouble, int> nullableRod(rawNullableMap);
    (void)nullableRod;
#else
    auto migrated10 =
        std::make_shared<ReadOnlyDictionary<NullableDouble, int>::MapType>();
    ReadOnlyDictionary<NullableDouble, int> nullableRod(migrated10);
    (void)nullableRod;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 11
    // NEGATIVE(frozenset-nullable-double-createfromset-raw): cannot convert
    //     | no matching function for call
    std::unordered_set<NullableDouble> rawNullableFrozenSet;
    (void)FrozenSet<NullableDouble>::CreateFromSet(rawNullableFrozenSet);
#else
    FrozenSet<NullableDouble>::SetType migrated11;
    (void)FrozenSet<NullableDouble>::CreateFromSet(migrated11);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 12
    // NEGATIVE(frozendict-nullable-double-createfrommap-raw): cannot convert
    //     | no matching function for call
    std::unordered_map<NullableDouble, int> rawNullableFrozenMap;
    (void)FrozenDictionary<NullableDouble, int>::CreateFromMap(rawNullableFrozenMap);
#else
    FrozenDictionary<NullableDouble, int>::MapType migrated12;
    (void)FrozenDictionary<NullableDouble, int>::CreateFromMap(migrated12);
#endif

    Dictionary<NullableDouble, int> nullableDictionary;
#if SHARP_RUNTIME_NEGATIVE_SITE == 13
    // NEGATIVE(dictionary-nullable-double-tomap-raw-reference): invalid initialization of reference
    //     | cannot bind
    const std::unordered_map<NullableDouble, int>& nullableBacking =
        nullableDictionary.ToMap();
    (void)nullableBacking;
#else
    const Dictionary<NullableDouble, int>::MapType& nullableBacking =
        nullableDictionary.ToMap();
    (void)nullableBacking;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 14
    // NEGATIVE(dictionary-nullable-double-maptype-must-not-be-raw): static assertion failed
    static_assert(std::is_same_v<Dictionary<NullableDouble, int>::MapType,
                                 std::unordered_map<NullableDouble, int>>,
                  "site 14 must be rejected: the nullable-floating map type must move");
#endif

    static_assert(std::is_same_v<FrozenSet<NullableDouble>::const_iterator,
                                 std::unordered_set<NullableDouble>::const_iterator>,
                  "measured: a nullable-double node iterator does not move");
    static_assert(!std::is_same_v<FrozenSet<NullableLongDouble>::const_iterator,
                                  std::unordered_set<NullableLongDouble>::const_iterator>,
                  "measured: a nullable-long-double node iterator moves");

#if SHARP_RUNTIME_NEGATIVE_SITE == 15
    // NEGATIVE(frozenset-nullable-longdouble-iterator-raw): cannot convert
    //     | conversion from
    FrozenSet<NullableLongDouble>::SetType nullableLongSet;
    auto nullableLongFrozen = FrozenSet<NullableLongDouble>::CreateFromSet(nullableLongSet);
    std::unordered_set<NullableLongDouble>::const_iterator nullableLongIterator =
        nullableLongFrozen.begin();
    (void)nullableLongIterator;
#else
    FrozenSet<NullableLongDouble>::SetType migrated15;
    auto nullableLongFrozen = FrozenSet<NullableLongDouble>::CreateFromSet(migrated15);
    FrozenSet<NullableLongDouble>::const_iterator nullableLongIterator =
        nullableLongFrozen.begin();
    (void)nullableLongIterator;
#endif

    // The whole non-floating surface is unchanged and must keep compiling in
    // every configuration, including the baseline.
    static_assert(std::is_same_v<Dictionary<int, int>::MapType,
                                 std::unordered_map<int, int>>);
    static_assert(std::is_same_v<HashSet<int>::SetType, std::unordered_set<int>>);
    static_assert(std::is_same_v<FrozenSet<int>::SetType, std::unordered_set<int>>);
    static_assert(std::is_same_v<FrozenDictionary<int, int>::MapType,
                                 std::unordered_map<int, int>>);
    static_assert(std::is_same_v<ReadOnlySet<int>::SetType, std::unordered_set<int>>);
    static_assert(std::is_same_v<ReadOnlyDictionary<int, int>::MapType,
                                 std::unordered_map<int, int>>);
    auto plainSet = std::make_shared<std::unordered_set<int>>();
    ReadOnlySet<int> plainRos(plainSet);
    auto plainMap = std::make_shared<std::unordered_map<int, int>>();
    ReadOnlyDictionary<int, int> plainRod(plainMap);
    (void)plainRos;
    (void)plainRod;

    return 0;
}
