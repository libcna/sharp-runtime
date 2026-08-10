// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

// =====================================================================================
// The ONE authoritative definition of the collection mutation-counter test seam.
// Ticket #1800 (REMED-COLL-VERSION-SEAM-ODR).
//
// `System/Collections/detail/MutationCounter.hpp` DECLARES
//
//     namespace SharpRuntime::Testing { template <typename TOwner> struct CollectionVersionAccess; }
//
// and never defines it, so no production translation unit and no consumer can
// name a complete type -- proved, not asserted, by
// `test/consumer/collections_mutation_version_negative.cpp`. Every collection
// that owns a counter befriends the seam for its own specialisation, and
// `detail::BasicMutationCounter` befriends every specialisation, so one seam
// name serves all fifteen collections.
//
// THIS FILE IS THE ONLY PLACE IN THE REPOSITORY THAT MAY DEFINE ANY
// SPECIALISATION OF THAT TEMPLATE. Before ticket #1800, five test translation
// units of the single `SharpRuntimeTests_Collections_Core` program each wrote
// their own copy, in two families:
//
//   * `SR1787_SEAM_BODY`  (CollectionVersionCounterTests.cpp)   -- version + positionVersion
//   * `SR1794_SEAM_BODY`  (four later suites)                   -- version only
//
// and the counter-level partial specialisation diverged the same way
// (`read` + `write` against `read` alone). Three specialisations therefore had
// two token-different definitions in one program, which is a one-definition-rule
// violation, ill-formed with no diagnostic required. It was measured, not
// theorised: at `-O0` the linker keeps whichever weak COMDAT definition it saw
// first and BOTH units execute it, so swapping two object files on the link line
// changed the answer; at `-O1` and above each unit inlines its own body and the
// two units disagree inside one process. Neither `ld`, nor `-flto -Wodr`, nor
// ASan with `detect_odr_violation=2`, nor UBSan said anything. The reproduction
// and the exact figures are in `docs/CollectionVersionTestSeamDesign.md`.
//
// RULES FOR ANY FUTURE SUITE THAT NEEDS THE COUNTER:
//
//   1. Include this header. Do NOT write `template<> struct
//      CollectionVersionAccess<...>` in a test translation unit -- if the unit
//      also includes this header the compiler rejects it as a redefinition, and
//      if it does not, `scripts/check_version_seam_odr.py` fails the build.
//   2. If a collection is missing below, add it HERE, once, through the
//      SHARP_RUNTIME_COLLECTION_VERSION_SEAM macro.
//   3. The seam grants access and defines no behaviour. Do not give it a method
//      that does anything a collection's own public API could not.
// =====================================================================================

#include <type_traits>
#include <utility>

#include "System/Collections/ArrayList.hpp"
#include "System/Collections/BitArray.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"
#include "System/Collections/Queue.hpp"
#include "System/Collections/Stack.hpp"
#include "System/Collections/detail/MutationCounter.hpp"
#include "System/Collections/Generic/Dictionary.hpp"
#include "System/Collections/Generic/HashSet.hpp"
#include "System/Collections/Generic/LinkedList.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/Generic/OrderedDictionary.hpp"
#include "System/Collections/Generic/Queue.hpp"
#include "System/Collections/Generic/SortedDictionary.hpp"
#include "System/Collections/Generic/SortedList.hpp"
#include "System/Collections/Generic/Stack.hpp"
#include "System/Collections/ObjectModel/Collection.hpp"

namespace SharpRuntime::Testing {

/**
 * The counter-level seam: the only code anywhere that touches
 * `BasicMutationCounter::value_`. Every collection-level specialisation below
 * delegates to it, so a collection never needs its own idea of how a counter is
 * read or written.
 */
template <typename V>
struct CollectionVersionAccess<System::Collections::detail::BasicMutationCounter<V>> {
    using Counter = System::Collections::detail::BasicMutationCounter<V>;

    /** Reads the counter without mutating it. */
    static V read(const Counter& c) { return c.getValueProperty(); }

    /**
     * Positions the counter at @p v. This is the only capability the seam adds
     * beyond observation, and it exists because the near-boundary regressions of
     * ticket #1787 would otherwise need 2^32 or 2^64 real mutations.
     */
    static void write(Counter& c, V v) { c.value_ = v; }
};

/**
 * The collection-level body. Each specialisation is a friend of exactly one
 * collection, reaches that collection's private `version_`, and defers to the
 * counter-level seam above.
 *
 * `positionVersion` is part of the canonical body for every collection even
 * though only `CollectionVersionCounterTests.cpp` calls it today: the
 * alternative -- two bodies, one with it and one without -- is precisely the
 * defect ticket #1800 closed.
 */
#define SHARP_RUNTIME_COLLECTION_VERSION_SEAM(...)                                         \
    using Coll = __VA_ARGS__;                                                              \
    using Counter = std::remove_cvref_t<decltype(std::declval<Coll&>().version_)>;         \
    using Seam = CollectionVersionAccess<Counter>;                                         \
    using value_type = typename Counter::value_type;                                       \
    static value_type version(const Coll& c) { return Seam::read(c.version_); }            \
    static void positionVersion(Coll& c, value_type v) { Seam::write(c.version_, v); }

template <typename T>
struct CollectionVersionAccess<System::Collections::Generic::List<T>> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::Generic::List<T>)
};
template <typename T>
struct CollectionVersionAccess<System::Collections::Generic::HashSet<T>> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::Generic::HashSet<T>)
};
template <typename K, typename V>
struct CollectionVersionAccess<System::Collections::Generic::Dictionary<K, V>> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::Generic::Dictionary<K, V>)
};
template <typename K, typename V>
struct CollectionVersionAccess<System::Collections::Generic::SortedDictionary<K, V>> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::Generic::SortedDictionary<K, V>)
};
template <typename K, typename V>
struct CollectionVersionAccess<System::Collections::Generic::SortedList<K, V>> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::Generic::SortedList<K, V>)
};
template <typename K, typename V>
struct CollectionVersionAccess<System::Collections::Generic::OrderedDictionary<K, V>> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::Generic::OrderedDictionary<K, V>)
};
template <typename T>
struct CollectionVersionAccess<System::Collections::Generic::LinkedList<T>> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::Generic::LinkedList<T>)
};
template <typename T>
struct CollectionVersionAccess<System::Collections::Generic::Queue<T>> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::Generic::Queue<T>)
};
template <typename T>
struct CollectionVersionAccess<System::Collections::Generic::Stack<T>> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::Generic::Stack<T>)
};
template <>
struct CollectionVersionAccess<System::Collections::ArrayList> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::ArrayList)
};
template <>
struct CollectionVersionAccess<System::Collections::Hashtable> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::Hashtable)
};
template <>
struct CollectionVersionAccess<System::Collections::ListDictionaryInternal> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::ListDictionaryInternal)
};
template <>
struct CollectionVersionAccess<System::Collections::Queue> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::Queue)
};
template <>
struct CollectionVersionAccess<System::Collections::Stack> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::Stack)
};
template <>
struct CollectionVersionAccess<System::Collections::BitArray> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::BitArray)
};
// Added by ticket #1791, which gave ObjectModel::Collection<T> the mutation counter it
// previously lacked so that its tracked indexer has something to advance.
template <typename T>
struct CollectionVersionAccess<System::Collections::ObjectModel::Collection<T>> {
    SHARP_RUNTIME_COLLECTION_VERSION_SEAM(System::Collections::ObjectModel::Collection<T>)
};

#undef SHARP_RUNTIME_COLLECTION_VERSION_SEAM

}  // namespace SharpRuntime::Testing
