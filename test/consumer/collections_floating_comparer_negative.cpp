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
using System::Collections::Generic::HashSet;
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

#if SHARP_RUNTIME_NEGATIVE_SITE == 8
    // NEGATIVE(hashset-double-iterator-raw): cannot convert
    //     | conversion from
    //     | no viable conversion
    //     | invalid conversion
    HashSet<double> hs;
    std::unordered_set<double>::iterator it = hs.begin();
    (void)it;
#else
    HashSet<double> hs;
    HashSet<double>::iterator it = hs.begin();
    (void)it;
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
