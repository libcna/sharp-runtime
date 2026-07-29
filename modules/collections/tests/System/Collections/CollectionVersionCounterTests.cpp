// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1787
// (REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP), the repository-wide sweep that ticket #1786
// opened after repairing SortedSet<T> alone.
//
// Fifteen collections in modules/collections/include/ carry a mutation counter that a
// fail-fast enumerator snapshots and compares for EQUALITY ONLY. Fourteen declared it
// SharpRuntime::intcs -- int32_t -- and BitArray declared it std::uint32_t. Three distinct
// defects followed, all reproduced against the committed pre-fix headers before anything
// changed (build-probe-collversion/probe2_defects.cpp, logs probe2_prefix_*.log):
//
//   1. `++version_` at INTCS_MAX is signed-integer overflow -- undefined behaviour.
//      FOURTEEN UBSan reports, one per collection, e.g.
//      "Generic/List.hpp:116:68: runtime error: signed integer overflow: 2147483647 + 1
//      cannot be represented in type 'int'". BitArray was already unsigned and is the one
//      exception.
//   2. 2^32 effective mutations later the counter returns to a value an outstanding
//      enumerator captured, and the guard silently accepts the stale enumerator. Fifteen
//      reproductions, one per collection.
//   3. The implicitly declared copy/move assignment operator transplanted the SOURCE's
//      counter into the destination, so an enumerator outstanding over the destination saw
//      no change even though every element it could refer to had just been destroyed. This
//      one needs no overflow at all -- the two counters merely have to match, which two
//      collections that have taken the same number of mutations routinely do. Six of the
//      fourteen reproduced as AddressSanitizer heap-use-after-free or heap-buffer-overflow
//      errors rather than merely wrong answers (HashSet, Dictionary, SortedDictionary,
//      OrderedDictionary, Hashtable, ListDictionaryInternal).
//
// #1786's defects 3 and 4 -- a cached Count keyed on the counter, and a `-1` "never
// computed" sentinel the counter itself could reach -- are specific to SortedSet<T>'s
// per-view Count cache. No other collection caches anything against its counter and none
// reserves a counter value, so neither applies here; that is asserted below rather than
// assumed.
//
// The repair replaces the bare integer field with
// System::Collections::detail::BasicMutationCounter, whose increment is unsigned (defined
// for every representable prior value) and whose ASSIGNMENT advances the destination
// instead of taking the source's value. Thirteen collections use the 64-bit
// MutationCounter. LinkedList<T> and BitArray kept the 32-bit NarrowMutationCounter because
// widening them would grow a public object (sizeof(LinkedList<int>) 40 -> 48,
// sizeof(BitArray::Enumerator) 32 -> 40), which needed user approval this repository had not
// been given; their residual 2^32 alias was pinned below so a future approved widening had
// to flip the assertion deliberately. Every measured sizeof, alignof, and counter offset was
// unchanged (build-probe-collversion/probe1_*_layout.log).
//
// TICKET #1788 PERFORMED EXACTLY THAT FLIP, FOR LinkedList<T> ONLY. The user approved the
// object growth, LinkedListAdapter::kNarrowCounter went true -> false, and every
// kNarrowCounter branch below therefore now takes the WIDE-family path for it: the counter is
// asserted 64-bit, and NoStaleSnapshotBecomesValidAcrossTheOld2Pow32Distance asserts a stale
// snapshot IS rejected instead of asserting that it is accepted. Measured:
// sizeof(LinkedList<int>) 40 -> 48, sizeof(Enumerator) UNCHANGED at 40 (the wider snapshot
// landed in padding it already had), zero LinkedList symbols added, removed or renamed. The
// dedicated suite is Generic/LinkedListVersionWideningTests.cpp; the record is
// docs/CollectionVersionCounterSweep.md section 8.1. BitArray is untouched -- ticket #1789
// remains blocked on its own, separate approval, so it is the one narrow adapter left here.
//
// Reaching a boundary through the public API would need billions of real mutations, so the
// near-boundary cases position the counter through the test-only access seam
// SharpRuntime::Testing::CollectionVersionAccess<T>, which detail/MutationCounter.hpp
// declares and every affected class befriends. It grants access and no behaviour; nothing
// in the library or in a consumer can reach it. No test below performs more than a few
// dozen real mutations.
//
// This file spelled its own copy of the seam (SR1787_SEAM_BODY) until ticket #1800, and
// four later suites spelled a second, DIFFERENT copy; the one definition now lives in
// ../../support/CollectionVersionSeam.hpp and this file merely includes it.
#include <gtest/gtest.h>

#include <any>
#include <cstddef>
#include <deque>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
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
#include "System/InvalidOperationException.hpp"

#include "../../support/CollectionVersionSeam.hpp"

using SharpRuntime::intcs;
using SharpRuntime::uintcs;
using SharpRuntime::ulongcs;

namespace NG = System::Collections;
namespace G = System::Collections::Generic;

namespace {

template<typename C>
using Seam = SharpRuntime::Testing::CollectionVersionAccess<C>;

template<typename C>
auto versionOf(const C& c) { return Seam<C>::version(c); }

template<typename C>
void positionVersion(C& c, typename Seam<C>::value_type v) { Seam<C>::positionVersion(c, v); }

constexpr ulongcs kOldInt32Max = 2147483647ULL;   // where the pre-fix increment was UB
constexpr ulongcs kOldAliasStep = 4294967296ULL;  // 2^32: where a 32-bit snapshot repeats

/**
 * Type-erased handle over any of the enumerator shapes this module returns
 * (IEnumerator*, IEnumerator<T>*, IDictionaryEnumerator*), so one typed test body can
 * drive all of them. Owns the heap enumerator its adapter allocated.
 */
class AnyEnumerator {
    std::function<bool()> moveNext_;
    std::function<void()> reset_;
    std::function<void()> destroy_;

public:
    template<typename E>
    explicit AnyEnumerator(E* e)
        : moveNext_([e] { return e->MoveNext(); }),
          reset_([e] { e->Reset(); }),
          destroy_([e] { delete e; }) {}

    AnyEnumerator(const AnyEnumerator&) = delete;
    AnyEnumerator& operator=(const AnyEnumerator&) = delete;
    AnyEnumerator(AnyEnumerator&&) = default;
    AnyEnumerator& operator=(AnyEnumerator&&) = delete;
    ~AnyEnumerator() { if (destroy_) destroy_(); }

    bool MoveNext() { return moveNext_(); }
    void Reset() { reset_(); }
};

// Stable, never-reused distinct addresses for the void*-keyed non-generic collections.
// std::deque never invalidates references to existing elements when it grows, so every
// address handed out stays valid for the life of the process.
int* slot(int i) {
    static std::deque<int> storage;
    while (static_cast<int>(storage.size()) <= i) storage.push_back(0);
    return &storage[static_cast<std::size_t>(i)];
}

// -------------------------------------------------------------------------------------
// Adapters: enumerator-shaped collections (fail-fast lives in MoveNext()/Reset()).
// -------------------------------------------------------------------------------------

struct ListAdapter {
    using Collection = G::List<int>;
    static constexpr bool kNarrowCounter = false;
    static constexpr bool kSelfAssignmentIsNoOp = false;
    static constexpr bool kHasNoOpMutation = true;   // Remove of an absent item
    static constexpr bool kHasClear = true;
    static const char* name() { return "List"; }
    static Collection make() { Collection c; c.Add(1); c.Add(2); c.Add(3); return c; }
    static Collection makeOther() { Collection c; c.Add(7); return c; }
    static Collection makeEmpty() { return Collection{}; }
    static void mutate(Collection& c) { static int n = 100; c.Add(n++); }
    static void noOpMutate(Collection& c) { EXPECT_FALSE(c.Remove(987654)); }
    static void clear(Collection& c) { c.Clear(); }
    static AnyEnumerator enumerate(Collection& c) { return AnyEnumerator(c.GetEnumerator()); }
};

struct SortedListAdapter {
    using Collection = G::SortedList<int, int>;
    static constexpr bool kNarrowCounter = false;
    static constexpr bool kSelfAssignmentIsNoOp = false;
    static constexpr bool kHasNoOpMutation = true;
    static constexpr bool kHasClear = true;
    static const char* name() { return "SortedList"; }
    static Collection make() { Collection c; c.Add(1, 1); c.Add(2, 2); c.Add(3, 3); return c; }
    static Collection makeOther() { Collection c; c.Add(7, 7); return c; }
    static Collection makeEmpty() { return Collection{}; }
    static void mutate(Collection& c) { static int n = 100; c.Add(n, n); ++n; }
    static void noOpMutate(Collection& c) { EXPECT_FALSE(c.Remove(987654)); }
    static void clear(Collection& c) { c.Clear(); }
    static AnyEnumerator enumerate(Collection& c) { return AnyEnumerator(c.GetEnumerator()); }
};

struct GenericQueueAdapter {
    using Collection = G::Queue<int>;
    static constexpr bool kNarrowCounter = false;
    static constexpr bool kSelfAssignmentIsNoOp = false;
    static constexpr bool kHasNoOpMutation = false;
    static constexpr bool kHasClear = true;
    static const char* name() { return "GenericQueue"; }
    static Collection make() { Collection c; c.Enqueue(1); c.Enqueue(2); c.Enqueue(3); return c; }
    static Collection makeOther() { Collection c; c.Enqueue(7); return c; }
    static Collection makeEmpty() { return Collection{}; }
    static void mutate(Collection& c) { static int n = 100; c.Enqueue(n++); }
    static void noOpMutate(Collection&) {}
    static void clear(Collection& c) { c.Clear(); }
    static AnyEnumerator enumerate(Collection& c) { return AnyEnumerator(c.GetEnumerator()); }
};

struct GenericStackAdapter {
    using Collection = G::Stack<int>;
    static constexpr bool kNarrowCounter = false;
    static constexpr bool kSelfAssignmentIsNoOp = false;
    static constexpr bool kHasNoOpMutation = false;
    static constexpr bool kHasClear = true;
    static const char* name() { return "GenericStack"; }
    static Collection make() { Collection c; c.Push(1); c.Push(2); c.Push(3); return c; }
    static Collection makeOther() { Collection c; c.Push(7); return c; }
    static Collection makeEmpty() { return Collection{}; }
    static void mutate(Collection& c) { static int n = 100; c.Push(n++); }
    static void noOpMutate(Collection&) {}
    static void clear(Collection& c) { c.Clear(); }
    static AnyEnumerator enumerate(Collection& c) { return AnyEnumerator(c.GetEnumerator()); }
};

struct ArrayListAdapter {
    using Collection = NG::ArrayList;
    static constexpr bool kNarrowCounter = false;
    static constexpr bool kSelfAssignmentIsNoOp = false;
    static constexpr bool kHasNoOpMutation = false;
    static constexpr bool kHasClear = true;
    static const char* name() { return "ArrayList"; }
    static Collection make() {
        Collection c; c.Add(std::any(1)); c.Add(std::any(2)); c.Add(std::any(3)); return c;
    }
    static Collection makeOther() { Collection c; c.Add(std::any(7)); return c; }
    static Collection makeEmpty() { return Collection{}; }
    static void mutate(Collection& c) { static int n = 100; c.Add(std::any(n++)); }
    static void noOpMutate(Collection&) {}
    static void clear(Collection& c) { c.Clear(); }
    static AnyEnumerator enumerate(Collection& c) { return AnyEnumerator(c.GetEnumerator()); }
};

struct HashtableAdapter {
    using Collection = NG::Hashtable;
    static constexpr bool kNarrowCounter = false;
    static constexpr bool kSelfAssignmentIsNoOp = false;
    // Ticket #1802: Remove of an absent key is a no-op and must move nothing.
    // Before it, all three Remove overloads were `_map.erase(k); ++version_;` and
    // this flag was false because the collection had no operation that could be
    // asked to change nothing -- not because Remove was correct. Matches
    // ListDictionaryAdapter, which ticket #1798 brought to the same rule.
    static constexpr bool kHasNoOpMutation = true;   // Remove of an absent key
    static constexpr bool kHasClear = true;
    static const char* name() { return "Hashtable"; }
    static Collection make() {
        Collection c;
        c.Add(std::string("a"), std::any(1));
        c.Add(std::string("b"), std::any(2));
        c.Add(std::string("c"), std::any(3));
        return c;
    }
    static Collection makeOther() { Collection c; c.Add(std::string("z"), std::any(7)); return c; }
    static Collection makeEmpty() { return Collection{}; }
    static void mutate(Collection& c) {
        static int n = 100;
        c.Add("k" + std::to_string(n++), std::any(1));
    }
    static void noOpMutate(Collection& c) { c.Remove(std::string("never-added")); }
    static void clear(Collection& c) { c.Clear(); }
    static AnyEnumerator enumerate(Collection& c) { return AnyEnumerator(c.GetEnumerator()); }
};

struct ListDictionaryAdapter {
    using Collection = NG::ListDictionaryInternal;
    static constexpr bool kNarrowCounter = false;
    static constexpr bool kSelfAssignmentIsNoOp = false;
    static constexpr bool kHasNoOpMutation = true;   // Remove of an absent key
    static constexpr bool kHasClear = true;
    static const char* name() { return "ListDictionaryInternal"; }
    static Collection make() {
        Collection c;
        for (int i = 0; i < 3; ++i) c.Add(slot(i), slot(32 + i));
        return c;
    }
    static Collection makeOther() { Collection c; c.Add(slot(20), slot(21)); return c; }
    static Collection makeEmpty() { return Collection{}; }
    static void mutate(Collection& c) { static int n = 40; c.Add(slot(n), slot(n + 1)); n += 2; }
    static void noOpMutate(Collection& c) { c.Remove(slot(63)); }
    static void clear(Collection& c) { c.Clear(); }
    static AnyEnumerator enumerate(Collection& c) { return AnyEnumerator(c.GetEnumerator()); }
};

struct NonGenericQueueAdapter {
    using Collection = NG::Queue;
    static constexpr bool kNarrowCounter = false;
    static constexpr bool kSelfAssignmentIsNoOp = false;
    static constexpr bool kHasNoOpMutation = false;
    static constexpr bool kHasClear = true;
    static const char* name() { return "NonGenericQueue"; }
    static Collection make() {
        Collection c; for (int i = 0; i < 3; ++i) c.Enqueue(slot(i)); return c;
    }
    static Collection makeOther() { Collection c; c.Enqueue(slot(20)); return c; }
    static Collection makeEmpty() { return Collection{}; }
    static void mutate(Collection& c) { static int n = 40; c.Enqueue(slot(n++)); }
    static void noOpMutate(Collection&) {}
    static void clear(Collection& c) { c.Clear(); }
    static AnyEnumerator enumerate(Collection& c) { return AnyEnumerator(c.GetEnumerator()); }
};

struct NonGenericStackAdapter {
    using Collection = NG::Stack;
    static constexpr bool kNarrowCounter = false;
    static constexpr bool kSelfAssignmentIsNoOp = false;
    static constexpr bool kHasNoOpMutation = false;
    static constexpr bool kHasClear = true;
    static const char* name() { return "NonGenericStack"; }
    static Collection make() {
        Collection c; for (int i = 0; i < 3; ++i) c.Push(slot(i)); return c;
    }
    static Collection makeOther() { Collection c; c.Push(slot(20)); return c; }
    static Collection makeEmpty() { return Collection{}; }
    static void mutate(Collection& c) { static int n = 40; c.Push(slot(n++)); }
    static void noOpMutate(Collection&) {}
    static void clear(Collection& c) { c.Clear(); }
    static AnyEnumerator enumerate(Collection& c) { return AnyEnumerator(c.GetEnumerator()); }
};

struct LinkedListAdapter {
    using Collection = G::LinkedList<int>;
    // Ticket #1788 widened this to 64 bits under explicit approval for sizeof(LinkedList<int>)
    // 40 -> 48. Flipping this flag is what converts every pinned 32-bit residual assertion
    // below into the wide-family assertion -- deliberately, which is the point of the flag.
    static constexpr bool kNarrowCounter = false;
    static constexpr bool kHasNoOpMutation = true;
    static constexpr bool kHasClear = true;
    // LinkedList<T> declares its own copy/move assignment (ticket #1769) and guards it with
    // `if (this != &other)`, so self-assignment genuinely changes nothing and must not
    // invalidate. Every other collection uses the implicit member-wise operator, where
    // self-assignment of the backing standard container may reallocate.
    static constexpr bool kSelfAssignmentIsNoOp = true;
    static const char* name() { return "LinkedList"; }
    static Collection make() {
        Collection c; c.AddLast(1); c.AddLast(2); c.AddLast(3); return c;
    }
    static Collection makeOther() { Collection c; c.AddLast(7); return c; }
    static Collection makeEmpty() { return Collection{}; }
    static void mutate(Collection& c) { static int n = 100; c.AddLast(n++); }
    static void noOpMutate(Collection& c) { EXPECT_FALSE(c.Remove(987654)); }
    static void clear(Collection& c) { c.Clear(); }
    static AnyEnumerator enumerate(Collection& c) { return AnyEnumerator(c.GetEnumerator()); }
};

struct BitArrayAdapter {
    using Collection = NG::BitArray;
    static constexpr bool kNarrowCounter = true;   // see MutationCounter.hpp / sweep doc §8
    static constexpr bool kSelfAssignmentIsNoOp = false;
    static constexpr bool kHasNoOpMutation = false;
    static constexpr bool kHasClear = false;       // BitArray has no Clear()
    static const char* name() { return "BitArray"; }
    static Collection make() { return Collection(8); }
    static Collection makeOther() { return Collection(8, true); }
    static Collection makeEmpty() { return Collection(intcs{0}); }
    static void mutate(Collection& c) { static int n = 0; c.Set(n % 8, (n % 2) == 0); ++n; }
    static void noOpMutate(Collection&) {}
    static void clear(Collection&) {}
    static AnyEnumerator enumerate(Collection& c) { return AnyEnumerator(c.GetEnumerator()); }
};

using EnumeratorAdapters = ::testing::Types<
    ListAdapter, SortedListAdapter, GenericQueueAdapter, GenericStackAdapter,
    ArrayListAdapter, HashtableAdapter, ListDictionaryAdapter, NonGenericQueueAdapter,
    NonGenericStackAdapter, LinkedListAdapter, BitArrayAdapter>;

struct AdapterName {
    template<typename T>
    static std::string GetName(int) { return T::name(); }
};

template<typename Adapter>
class CollectionVersionCounter : public ::testing::Test {};

TYPED_TEST_SUITE(CollectionVersionCounter, EnumeratorAdapters, AdapterName);

// --- the fail-fast contract ticket 1713 established, which this ticket must preserve -----

TYPED_TEST(CollectionVersionCounter, OrdinaryMutationInvalidatesAnOutstandingEnumerator) {
    auto c = TypeParam::make();
    auto e = TypeParam::enumerate(c);
    EXPECT_TRUE(e.MoveNext());
    TypeParam::mutate(c);
    EXPECT_THROW(e.MoveNext(), System::InvalidOperationException);
}

TYPED_TEST(CollectionVersionCounter, ResetAlsoFailsFastAfterAMutation) {
    auto c = TypeParam::make();
    auto e = TypeParam::enumerate(c);
    EXPECT_TRUE(e.MoveNext());
    TypeParam::mutate(c);
    EXPECT_THROW(e.Reset(), System::InvalidOperationException);
}

TYPED_TEST(CollectionVersionCounter, ARejectedOrAbsentMutationInvalidatesNothing) {
    auto c = TypeParam::make();
    const auto before = versionOf(c);
    auto e = TypeParam::enumerate(c);
    EXPECT_TRUE(e.MoveNext());
    if constexpr (TypeParam::kHasNoOpMutation) {
        TypeParam::noOpMutate(c);
        EXPECT_EQ(versionOf(c), before);
        EXPECT_NO_THROW(e.MoveNext());
    } else {
        EXPECT_EQ(versionOf(c), before);
    }
}

TYPED_TEST(CollectionVersionCounter, ClearInvalidatesAnOutstandingEnumerator) {
    if constexpr (TypeParam::kHasClear) {
        auto c = TypeParam::make();
        auto e = TypeParam::enumerate(c);
        EXPECT_TRUE(e.MoveNext());
        TypeParam::clear(c);
        EXPECT_THROW(e.MoveNext(), System::InvalidOperationException);
    } else {
        SUCCEED() << TypeParam::name() << " has no Clear()";
    }
}

TYPED_TEST(CollectionVersionCounter, ReadingWithoutMutatingNeverInvalidates) {
    auto c = TypeParam::make();
    auto e = TypeParam::enumerate(c);
    const auto before = versionOf(c);
    while (e.MoveNext()) {}
    EXPECT_EQ(versionOf(c), before);
}

// --- defect 1: the increment is defined at and past the old int32 boundary ---------------

TYPED_TEST(CollectionVersionCounter, TheCounterIsUnsigned) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    static_assert(std::is_unsigned_v<Value>,
                  "an unsigned counter is what makes the increment defined at its maximum");
    EXPECT_EQ(std::is_signed_v<Value>, false);
}

TYPED_TEST(CollectionVersionCounter, TheCounterHasTheWidthItsLayoutPermits) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    if constexpr (TypeParam::kNarrowCounter) {
        // BitArray keeps 32 bits because widening its PUBLIC nested Enumerator grows a public
        // object; ticket #1789 holds the approval request. LinkedList<T> was here too until
        // #1788 was approved and flipped its adapter flag.
        EXPECT_EQ(sizeof(Value), sizeof(uintcs));
    } else {
        EXPECT_EQ(sizeof(Value), sizeof(ulongcs));
    }
}

TYPED_TEST(CollectionVersionCounter, IncrementAtTheOldInt32BoundaryMovesForward) {
    auto c = TypeParam::make();
    positionVersion(c, static_cast<typename Seam<typename TypeParam::Collection>::value_type>(
                           kOldInt32Max));
    TypeParam::mutate(c);
    EXPECT_EQ(versionOf(c), kOldInt32Max + 1);
}

TYPED_TEST(CollectionVersionCounter, MutationAtSevenBoundaryValuesNeverThrowsOrGoesBackwards) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    const ulongcs starts[] = {0, 1, kOldInt32Max - 1, kOldInt32Max, kOldInt32Max + 1,
                              static_cast<ulongcs>(std::numeric_limits<uintcs>::max()) - 1,
                              static_cast<ulongcs>(std::numeric_limits<uintcs>::max())};
    for (const ulongcs start : starts) {
        if (start > static_cast<ulongcs>(std::numeric_limits<Value>::max())) continue;
        auto c = TypeParam::make();
        positionVersion(c, static_cast<Value>(start));
        ASSERT_NO_THROW(TypeParam::mutate(c));
        EXPECT_EQ(versionOf(c), static_cast<Value>(start + 1)) << "start=" << start;
    }
}

TYPED_TEST(CollectionVersionCounter, ExhaustionWrapsToZeroWithoutUndefinedBehaviour) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    auto c = TypeParam::make();
    positionVersion(c, std::numeric_limits<Value>::max());
    auto e = TypeParam::enumerate(c);
    EXPECT_TRUE(e.MoveNext());
    ASSERT_NO_THROW(TypeParam::mutate(c));
    EXPECT_EQ(versionOf(c), Value{0});
    // The snapshot was taken at max, so the wrapped counter still differs from it.
    EXPECT_THROW(e.MoveNext(), System::InvalidOperationException);
}

// --- defect 2: no stale snapshot is revalidated across the old 2^32 alias ----------------

TYPED_TEST(CollectionVersionCounter, NoStaleSnapshotBecomesValidAcrossTheOld2Pow32Distance) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    auto c = TypeParam::make();
    auto e = TypeParam::enumerate(c);
    EXPECT_TRUE(e.MoveNext());
    const ulongcs snapshot = versionOf(c);
    TypeParam::mutate(c);

    if constexpr (TypeParam::kNarrowCounter) {
        // Documented, approval-blocked residual: a 32-bit counter genuinely does return to
        // the snapshot after 2^32 effective mutations, and the guard genuinely stops firing.
        // Pinned here so that widening BitArray -- which needs approval for a public
        // object-size change -- has to flip this assertion on purpose rather than by
        // accident. LinkedList<T> reached the `else` branch below when ticket #1788 was
        // approved and did exactly that. See docs/CollectionVersionCounterSweep.md §8.
        //
        // The distance is spelled `snapshot + 2^32` and NOT simply `snapshot`, even though a
        // 32-bit cast makes the two identical. #1788's mutation check measured why that
        // matters: written as `snapshot`, this assertion holds for a counter of ANY width and
        // therefore pins nothing about the residual it claims to describe -- flipping the
        // adapter flag left it passing either way, and only
        // TheCounterHasTheWidthItsLayoutPermits actually failed. Written this way, the
        // narrowing is what makes the guard silent, which is the claim.
        positionVersion(c, static_cast<Value>(snapshot + kOldAliasStep));
        EXPECT_EQ(static_cast<ulongcs>(versionOf(c)), snapshot)
            << TypeParam::name() << "'s 32-bit counter must truncate one full lap back onto "
                                    "the snapshot; that IS the residual";
        EXPECT_NO_THROW(e.MoveNext())
            << TypeParam::name() << " still carries the 32-bit alias by design";
    } else {
        for (const ulongcs multiple : {1ULL, 2ULL, 7ULL}) {
            auto fresh = TypeParam::make();
            auto stale = TypeParam::enumerate(fresh);
            EXPECT_TRUE(stale.MoveNext());
            const ulongcs base = versionOf(fresh);
            positionVersion(fresh, static_cast<Value>(base + multiple * kOldAliasStep));
            EXPECT_THROW(stale.MoveNext(), System::InvalidOperationException)
                << "multiple=" << multiple;
        }
    }
}

TYPED_TEST(CollectionVersionCounter, AnEnumeratorTakenAtTheBoundaryIsInvalidatedByTheNextMutation) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    auto c = TypeParam::make();
    positionVersion(c, static_cast<Value>(kOldInt32Max));
    auto e = TypeParam::enumerate(c);
    EXPECT_TRUE(e.MoveNext());
    TypeParam::mutate(c);
    EXPECT_THROW(e.MoveNext(), System::InvalidOperationException);
}

TYPED_TEST(CollectionVersionCounter, MutationPastTheOldBoundaryStaysExactAndKeepsInvalidating) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    auto c = TypeParam::make();
    const ulongcs start = TypeParam::kNarrowCounter ? kOldInt32Max : kOldAliasStep;
    positionVersion(c, static_cast<Value>(start));
    for (int i = 0; i < 25; ++i) {
        auto e = TypeParam::enumerate(c);
        EXPECT_TRUE(e.MoveNext());
        TypeParam::mutate(c);
        EXPECT_EQ(versionOf(c), static_cast<Value>(start + static_cast<ulongcs>(i) + 1));
        EXPECT_THROW(e.MoveNext(), System::InvalidOperationException) << "step " << i;
    }
}

// --- defect 3: assignment must not transplant the source's counter -----------------------

TYPED_TEST(CollectionVersionCounter, CopyAssignmentInvalidatesEnumeratorsOverTheDestination) {
    auto a = TypeParam::make();
    auto b = TypeParam::makeOther();
    auto e = TypeParam::enumerate(a);
    EXPECT_TRUE(e.MoveNext());
    const auto before = versionOf(a);
    a = b;
    EXPECT_GT(versionOf(a), before);
    EXPECT_THROW(e.MoveNext(), System::InvalidOperationException);
}

TYPED_TEST(CollectionVersionCounter, CopyAssignmentStillReplacesTheContents) {
    auto a = TypeParam::make();
    auto b = TypeParam::makeOther();
    a = b;
    auto ea = TypeParam::enumerate(a);
    auto eb = TypeParam::enumerate(b);
    int countA = 0;
    int countB = 0;
    while (ea.MoveNext()) ++countA;
    while (eb.MoveNext()) ++countB;
    EXPECT_EQ(countA, countB);
}

TYPED_TEST(CollectionVersionCounter, MoveAssignmentInvalidatesEnumeratorsOverTheDestination) {
    auto a = TypeParam::make();
    auto b = TypeParam::makeOther();
    auto e = TypeParam::enumerate(a);
    EXPECT_TRUE(e.MoveNext());
    const auto before = versionOf(a);
    a = std::move(b);
    EXPECT_GT(versionOf(a), before);
    EXPECT_THROW(e.MoveNext(), System::InvalidOperationException);
}

TYPED_TEST(CollectionVersionCounter, AssignmentWithAMatchingSourceCounterStillInvalidates) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    auto a = TypeParam::make();
    auto b = TypeParam::makeOther();
    auto e = TypeParam::enumerate(a);
    EXPECT_TRUE(e.MoveNext());
    const Value snapshot = versionOf(a);
    // The pre-fix defect: with the counters equal, copying b's value into a left the
    // enumerator apparently valid over storage that had just been destroyed.
    positionVersion(b, snapshot);
    a = b;
    EXPECT_NE(versionOf(a), snapshot);
    EXPECT_THROW(e.MoveNext(), System::InvalidOperationException);
}

TYPED_TEST(CollectionVersionCounter, SelfAssignmentBehavesAsThatCollectionDocuments) {
    auto a = TypeParam::make();
    auto e = TypeParam::enumerate(a);
    EXPECT_TRUE(e.MoveNext());
    const auto before = versionOf(a);
    a = a;  // NOLINT(clang-diagnostic-self-assign-overloaded)
    if constexpr (TypeParam::kSelfAssignmentIsNoOp) {
        // LinkedList<T>'s own operator= short-circuits on self-assignment, so nothing is
        // destroyed and nothing must be invalidated.
        EXPECT_EQ(versionOf(a), before);
        EXPECT_NO_THROW(e.MoveNext());
    } else {
        // Deliberate and conservative: member-wise self-assignment of the backing standard
        // container may reallocate the nodes an outstanding iterator points at, so failing
        // fast is the memory-safe answer. Documented in detail/MutationCounter.hpp.
        EXPECT_GT(versionOf(a), before);
        EXPECT_THROW(e.MoveNext(), System::InvalidOperationException);
    }
}

TYPED_TEST(CollectionVersionCounter, CopyConstructionLeavesTheSourceEnumerable) {
    auto a = TypeParam::make();
    auto e = TypeParam::enumerate(a);
    EXPECT_TRUE(e.MoveNext());
    const auto before = versionOf(a);
    auto copy = a;
    EXPECT_EQ(versionOf(a), before);
    EXPECT_NO_THROW(e.MoveNext());
    // The copy is a fresh object; its own enumerator is valid from the start.
    auto ec = TypeParam::enumerate(copy);
    EXPECT_TRUE(ec.MoveNext());
}

TYPED_TEST(CollectionVersionCounter, AssignmentDoesNotDisturbTheSourcesOwnEnumerators) {
    auto a = TypeParam::make();
    auto b = TypeParam::makeOther();
    auto eb = TypeParam::enumerate(b);
    EXPECT_TRUE(eb.MoveNext());
    a = b;
    EXPECT_NO_THROW(eb.MoveNext());
}

// --- empty and single-element collections ------------------------------------------------

TYPED_TEST(CollectionVersionCounter, AnEmptyCollectionEnumeratesToCompletionAndStaysExact) {
    auto c = TypeParam::makeEmpty();
    const auto before = versionOf(c);
    auto e = TypeParam::enumerate(c);
    EXPECT_FALSE(e.MoveNext());
    EXPECT_EQ(versionOf(c), before);
}

TYPED_TEST(CollectionVersionCounter, AMutationOnAnEmptyCollectionInvalidatesItsEnumerator) {
    auto c = TypeParam::makeEmpty();
    auto e = TypeParam::enumerate(c);
    if constexpr (std::is_same_v<TypeParam, BitArrayAdapter>) {
        SUCCEED() << "a zero-length BitArray has no bit to Set";
    } else {
        TypeParam::mutate(c);
        EXPECT_THROW(e.MoveNext(), System::InvalidOperationException);
    }
}

TYPED_TEST(CollectionVersionCounter, AFreshCollectionStartsCountingFromZero) {
    typename TypeParam::Collection empty = TypeParam::makeEmpty();
    EXPECT_EQ(versionOf(empty), 0u);
}

// --- no collection outside SortedSet<T> caches anything against its counter ---------------

TYPED_TEST(CollectionVersionCounter, NoCounterValueIsReservedAsASentinel) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    // #1786's defect 4 was a `-1` "never computed" Count-cache sentinel that the counter
    // itself could reach. These collections reserve nothing: every counter value, including
    // the one the pre-fix sentinel occupied and the maximum, behaves identically.
    const ulongcs probes[] = {0, 1, kOldInt32Max, kOldAliasStep - 1,
                              static_cast<ulongcs>(std::numeric_limits<Value>::max())};
    int expected = 0;
    {
        auto reference = TypeParam::make();
        auto e = TypeParam::enumerate(reference);
        while (e.MoveNext()) ++expected;
    }
    EXPECT_GT(expected, 0);
    for (const ulongcs v : probes) {
        if (v > static_cast<ulongcs>(std::numeric_limits<Value>::max())) continue;
        auto c = TypeParam::make();
        positionVersion(c, static_cast<Value>(v));
        auto e = TypeParam::enumerate(c);
        int seen = 0;
        while (e.MoveNext()) ++seen;
        EXPECT_EQ(seen, expected) << "counter positioned at " << v;
    }
}

// =====================================================================================
// Iterator-shaped collections: the guard lives in operator*/operator++ of a
// version-checked begin()/end() iterator rather than in an IEnumerator.
// =====================================================================================

struct HashSetAdapter {
    using Collection = G::HashSet<int>;
    static const char* name() { return "HashSet"; }
    static Collection make() { Collection c; c.Add(1); c.Add(2); c.Add(3); return c; }
    static Collection makeOther() { Collection c; c.Add(7); return c; }
    static void mutate(Collection& c) { static int n = 100; c.Add(n++); }
    static void noOpMutate(Collection& c) { EXPECT_FALSE(c.Add(1)); EXPECT_FALSE(c.Remove(987654)); }
    static void clear(Collection& c) { c.Clear(); }
    static void touch(Collection& c, typename Collection::iterator& it) { (void)c; (void)*it; }
};

struct DictionaryAdapter {
    using Collection = G::Dictionary<int, int>;
    static const char* name() { return "Dictionary"; }
    static Collection make() { Collection c; c.Add(1, 1); c.Add(2, 2); c.Add(3, 3); return c; }
    static Collection makeOther() { Collection c; c.Add(7, 7); return c; }
    static void mutate(Collection& c) { static int n = 100; c.Add(n, n); ++n; }
    static void noOpMutate(Collection& c) { EXPECT_FALSE(c.Remove(987654)); }
    static void clear(Collection& c) { c.Clear(); }
    static void touch(Collection& c, typename Collection::iterator& it) { (void)c; (void)*it; }
};

struct SortedDictionaryAdapter {
    using Collection = G::SortedDictionary<int, int>;
    static const char* name() { return "SortedDictionary"; }
    static Collection make() { Collection c; c.Add(1, 1); c.Add(2, 2); c.Add(3, 3); return c; }
    static Collection makeOther() { Collection c; c.Add(7, 7); return c; }
    static void mutate(Collection& c) { static int n = 100; c.Add(n, n); ++n; }
    static void noOpMutate(Collection& c) { EXPECT_FALSE(c.Remove(987654)); }
    static void clear(Collection& c) { c.Clear(); }
    static void touch(Collection& c, typename Collection::Iterator& it) { (void)c; (void)*it; }
};

struct OrderedDictionaryAdapter {
    using Collection = G::OrderedDictionary<int, int>;
    static const char* name() { return "OrderedDictionary"; }
    static Collection make() { Collection c; c.Add(1, 1); c.Add(2, 2); c.Add(3, 3); return c; }
    static Collection makeOther() { Collection c; c.Add(7, 7); return c; }
    static void mutate(Collection& c) { static int n = 100; c.Add(n, n); ++n; }
    static void noOpMutate(Collection& c) {
        EXPECT_FALSE(c.Remove(987654));
        EXPECT_FALSE(c.TryAdd(1, 5));
    }
    static void clear(Collection& c) { c.Clear(); }
    static void touch(Collection& c, typename Collection::Iterator& it) { (void)c; (void)*it; }
};

using IteratorAdapters = ::testing::Types<
    HashSetAdapter, DictionaryAdapter, SortedDictionaryAdapter, OrderedDictionaryAdapter>;

template<typename Adapter>
class CollectionIteratorVersion : public ::testing::Test {};

TYPED_TEST_SUITE(CollectionIteratorVersion, IteratorAdapters, AdapterName);

TYPED_TEST(CollectionIteratorVersion, OrdinaryMutationInvalidatesAnOutstandingIterator) {
    auto c = TypeParam::make();
    auto it = c.begin();
    EXPECT_NO_THROW(TypeParam::touch(c, it));
    TypeParam::mutate(c);
    EXPECT_THROW(TypeParam::touch(c, it), System::InvalidOperationException);
}

TYPED_TEST(CollectionIteratorVersion, ARejectedOrAbsentMutationInvalidatesNothing) {
    auto c = TypeParam::make();
    auto it = c.begin();
    const auto before = versionOf(c);
    TypeParam::noOpMutate(c);
    EXPECT_EQ(versionOf(c), before);
    EXPECT_NO_THROW(TypeParam::touch(c, it));
}

TYPED_TEST(CollectionIteratorVersion, ClearInvalidatesAnOutstandingIterator) {
    auto c = TypeParam::make();
    auto it = c.begin();
    TypeParam::clear(c);
    EXPECT_THROW(TypeParam::touch(c, it), System::InvalidOperationException);
}

TYPED_TEST(CollectionIteratorVersion, TheCounterIsUnsignedAndSixtyFourBits) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    static_assert(std::is_unsigned_v<Value>);
    EXPECT_EQ(sizeof(Value), sizeof(ulongcs));
}

TYPED_TEST(CollectionIteratorVersion, IncrementAtTheOldInt32BoundaryMovesForward) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    auto c = TypeParam::make();
    positionVersion(c, static_cast<Value>(kOldInt32Max));
    TypeParam::mutate(c);
    EXPECT_EQ(versionOf(c), kOldInt32Max + 1);
}

TYPED_TEST(CollectionIteratorVersion, NoStaleIteratorBecomesValidAcrossTheOld2Pow32Distance) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    for (const ulongcs multiple : {1ULL, 2ULL, 5ULL}) {
        auto c = TypeParam::make();
        auto it = c.begin();
        const ulongcs base = versionOf(c);
        positionVersion(c, static_cast<Value>(base + multiple * kOldAliasStep));
        EXPECT_THROW(TypeParam::touch(c, it), System::InvalidOperationException)
            << "multiple=" << multiple;
    }
}

TYPED_TEST(CollectionIteratorVersion, AnIteratorTakenAtTheBoundaryIsInvalidatedByTheNextMutation) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    auto c = TypeParam::make();
    positionVersion(c, static_cast<Value>(kOldInt32Max));
    auto it = c.begin();
    EXPECT_NO_THROW(TypeParam::touch(c, it));
    TypeParam::mutate(c);
    EXPECT_THROW(TypeParam::touch(c, it), System::InvalidOperationException);
}

TYPED_TEST(CollectionIteratorVersion, MutationPastTheOldBoundaryStaysExact) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    auto c = TypeParam::make();
    positionVersion(c, static_cast<Value>(kOldAliasStep));
    for (int i = 0; i < 25; ++i) {
        auto it = c.begin();
        TypeParam::mutate(c);
        EXPECT_EQ(versionOf(c), kOldAliasStep + static_cast<ulongcs>(i) + 1);
        EXPECT_THROW(TypeParam::touch(c, it), System::InvalidOperationException) << "step " << i;
    }
}

TYPED_TEST(CollectionIteratorVersion, CopyAssignmentInvalidatesIteratorsOverTheDestination) {
    auto a = TypeParam::make();
    auto b = TypeParam::makeOther();
    auto it = a.begin();
    const auto before = versionOf(a);
    a = b;
    EXPECT_GT(versionOf(a), before);
    EXPECT_THROW(TypeParam::touch(a, it), System::InvalidOperationException);
}

TYPED_TEST(CollectionIteratorVersion, MoveAssignmentInvalidatesIteratorsOverTheDestination) {
    auto a = TypeParam::make();
    auto b = TypeParam::makeOther();
    auto it = a.begin();
    const auto before = versionOf(a);
    a = std::move(b);
    EXPECT_GT(versionOf(a), before);
    EXPECT_THROW(TypeParam::touch(a, it), System::InvalidOperationException);
}

TYPED_TEST(CollectionIteratorVersion, AssignmentWithAMatchingSourceCounterStillInvalidates) {
    using Value = typename Seam<typename TypeParam::Collection>::value_type;
    auto a = TypeParam::make();
    auto b = TypeParam::makeOther();
    auto it = a.begin();
    const Value snapshot = versionOf(a);
    positionVersion(b, snapshot);
    a = b;
    EXPECT_NE(versionOf(a), snapshot);
    EXPECT_THROW(TypeParam::touch(a, it), System::InvalidOperationException);
}

TYPED_TEST(CollectionIteratorVersion, SelfAssignmentInvalidatesConservatively) {
    auto a = TypeParam::make();
    auto it = a.begin();
    const auto before = versionOf(a);
    a = a;  // NOLINT(clang-diagnostic-self-assign-overloaded)
    EXPECT_GT(versionOf(a), before);
    EXPECT_THROW(TypeParam::touch(a, it), System::InvalidOperationException);
}

TYPED_TEST(CollectionIteratorVersion, CopyConstructionLeavesTheSourceIterable) {
    auto a = TypeParam::make();
    auto it = a.begin();
    const auto before = versionOf(a);
    auto copy = a;
    EXPECT_EQ(versionOf(a), before);
    EXPECT_NO_THROW(TypeParam::touch(a, it));
}

TYPED_TEST(CollectionIteratorVersion, AnEmptyCollectionIteratesToCompletion) {
    typename TypeParam::Collection c;
    EXPECT_TRUE(c.begin() == c.end());
    EXPECT_EQ(versionOf(c), 0u);
}

TYPED_TEST(CollectionIteratorVersion, RangeForOverAnUnmutatedCollectionSeesEveryElement) {
    auto c = TypeParam::make();
    int seen = 0;
    for (const auto& entry : c) { (void)entry; ++seen; }
    EXPECT_EQ(seen, 3);
}

// =====================================================================================
// Non-trivial element types and per-type specifics.
// =====================================================================================

TEST(CollectionVersionCounterElementTypes, ListOfStringsInvalidatesAcrossTheOldBoundary) {
    G::List<std::string> c;
    c.Add("alpha");
    c.Add("beta");
    positionVersion(c, kOldInt32Max);
    auto* e = c.GetEnumerator();
    EXPECT_TRUE(e->MoveNext());
    c.Add("gamma");
    EXPECT_EQ(versionOf(c), kOldInt32Max + 1);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    delete e;
}

TEST(CollectionVersionCounterElementTypes, DictionaryOfStringsInvalidatesOnAssignment) {
    G::Dictionary<std::string, std::string> a;
    a.Add("k", "v");
    G::Dictionary<std::string, std::string> b;
    b.Add("other", "value");
    auto it = a.begin();
    a = b;
    EXPECT_THROW((void)*it, System::InvalidOperationException);
}

TEST(CollectionVersionCounterElementTypes, SortedDictionaryWithAnOrderOnlyKeyStillInvalidates) {
    struct LessOnly {
        int v;
        bool operator<(const LessOnly& other) const { return v < other.v; }
    };
    G::SortedDictionary<LessOnly, int> c;
    c.Add(LessOnly{1}, 1);
    c.Add(LessOnly{2}, 2);
    positionVersion(c, kOldAliasStep + 5);
    auto it = c.begin();
    c.Add(LessOnly{3}, 3);
    EXPECT_EQ(versionOf(c), kOldAliasStep + 6);
    EXPECT_THROW((void)*it, System::InvalidOperationException);
}

TEST(CollectionVersionCounterSpecifics, DictionaryIndexerInsertBumpsButOverwriteDoesNot) {
    G::Dictionary<int, int> c;
    c.Add(1, 1);
    const auto afterAdd = versionOf(c);
    c[1] = 42;                       // existing key: not a structural change
    EXPECT_EQ(versionOf(c), afterAdd);
    c[2] = 7;                        // new key: structural
    EXPECT_EQ(versionOf(c), afterAdd + 1);
}

TEST(CollectionVersionCounterSpecifics, LinkedListAssignmentAlreadyBumpedAndStillDoes) {
    // LinkedList<T> is the one collection that already declared its own copy/move
    // assignment (ticket #1769) and already bumped, so it never had defect 3. Pinned so a
    // future refactor cannot silently drop the bump. Ticket #1788 widened the counter this
    // reads and changed nothing about the bump itself.
    G::LinkedList<int> a;
    a.AddLast(1);
    a.AddLast(2);
    G::LinkedList<int> b;
    b.AddLast(7);
    auto* e = a.GetEnumerator();
    EXPECT_TRUE(e->MoveNext());
    const auto before = versionOf(a);
    a = b;
    EXPECT_GT(versionOf(a), before);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    delete e;
}

TEST(CollectionVersionCounterSpecifics, BitArraySetAllAndLengthChangesInvalidate) {
    NG::BitArray c(4);
    auto* e = c.GetEnumerator();
    EXPECT_TRUE(e->MoveNext());
    c.SetAll(true);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    delete e;

    auto* e2 = c.GetEnumerator();
    EXPECT_TRUE(e2->MoveNext());
    c.setLengthProperty(8);
    EXPECT_THROW(e2->MoveNext(), System::InvalidOperationException);
    delete e2;
}

TEST(CollectionVersionCounterSpecifics, HashtableKeyAndValueViewsInheritTheTablesFailFast) {
    NG::Hashtable t;
    t.Add(std::string("a"), std::any(1));
    t.Add(std::string("b"), std::any(2));
    auto* keys = t.getKeysProperty();   // caller owns the view, as ticket #1775 defined
    auto* e = keys->GetEnumerator();
    EXPECT_TRUE(e->MoveNext());
    t.Add(std::string("c"), std::any(3));
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    delete e;
    delete keys;
}

// =====================================================================================
// Source, layout, and value-semantics compatibility.
// =====================================================================================

TEST(CollectionVersionCounterCompatibility, TheCounterTypeItselfBehavesAsSpecified) {
    using System::Collections::detail::MutationCounter;
    using System::Collections::detail::NarrowMutationCounter;
    using Access = SharpRuntime::Testing::CollectionVersionAccess<MutationCounter>;

    static_assert(std::is_same_v<MutationCounter::value_type, ulongcs>);
    static_assert(std::is_same_v<NarrowMutationCounter::value_type, uintcs>);
    static_assert(sizeof(MutationCounter) == sizeof(ulongcs));
    static_assert(sizeof(NarrowMutationCounter) == sizeof(uintcs));
    static_assert(alignof(MutationCounter) == alignof(ulongcs));
    static_assert(std::is_nothrow_default_constructible_v<MutationCounter>);
    static_assert(std::is_nothrow_copy_constructible_v<MutationCounter>);
    static_assert(std::is_nothrow_copy_assignable_v<MutationCounter>);
    static_assert(std::is_nothrow_move_assignable_v<MutationCounter>);

    MutationCounter c;
    EXPECT_EQ(c.getValueProperty(), 0u);
    ++c;
    EXPECT_EQ(c.getValueProperty(), 1u);

    // Copy CONSTRUCTION inherits the value; a fresh object has no enumerators to invalidate.
    MutationCounter copy = c;
    EXPECT_EQ(copy.getValueProperty(), 1u);

    // ASSIGNMENT advances the destination instead of taking the source's value.
    MutationCounter dest;
    Access::write(dest, 41);
    MutationCounter src;
    Access::write(src, 41);
    dest = src;
    EXPECT_EQ(dest.getValueProperty(), 42u);
    dest = std::move(src);
    EXPECT_EQ(dest.getValueProperty(), 43u);

    // Exhaustion is defined: the maximum wraps to zero rather than being UB.
    Access::write(dest, std::numeric_limits<ulongcs>::max());
    ++dest;
    EXPECT_EQ(dest.getValueProperty(), 0u);
}

TEST(CollectionVersionCounterCompatibility, NoCounterOrCounterTypeLeaksThroughAPublicSurface) {
    // getCountProperty() still returns a plain intcs by value on every affected collection;
    // no 64-bit counter and no counter class appears in a public signature.
    static_assert(std::is_same_v<decltype(std::declval<const G::List<int>&>()
                                              .getCountProperty()), intcs>);
    static_assert(std::is_same_v<decltype(std::declval<const G::HashSet<int>&>()
                                              .getCountProperty()), intcs>);
    static_assert(std::is_same_v<decltype(std::declval<const NG::ArrayList&>()
                                              .getCountProperty()), intcs>);
    static_assert(std::is_same_v<decltype(std::declval<const NG::Hashtable&>()
                                              .getCountProperty()), intcs>);
    static_assert(std::is_same_v<decltype(std::declval<const NG::BitArray&>()
                                              .getLengthProperty()), intcs>);
    SUCCEED();
}

TEST(CollectionVersionCounterCompatibility, ValueSemanticsTraitsAreUnchanged) {
    static_assert(std::is_copy_constructible_v<G::List<int>>);
    static_assert(std::is_copy_assignable_v<G::List<int>>);
    static_assert(std::is_move_constructible_v<G::List<int>>);
    static_assert(std::is_move_assignable_v<G::List<int>>);
    static_assert(std::is_copy_assignable_v<G::Dictionary<int, int>>);
    static_assert(std::is_copy_assignable_v<NG::ArrayList>);
    static_assert(std::is_copy_assignable_v<NG::BitArray>);
    static_assert(!std::is_polymorphic_v<G::HashSet<int>>);
    static_assert(std::is_polymorphic_v<NG::ArrayList>);
    SUCCEED();
}

TEST(CollectionVersionCounterCompatibility, PublishedObjectSizesAreUnchanged) {
    // The published figures are LP64. The repository's rule against permanent
    // architecture-specific sizeof assertions is respected by the guard.
    //
    // G::LinkedList<int> is deliberately ABSENT from this list. It was 40 here until ticket
    // #1788, which grew it to 48 under explicit user approval -- the one figure in this
    // ticket's inventory that did NOT stay unchanged, so asserting it under a test named
    // "AreUnchanged" would be false. Its new size is pinned in
    // Generic/LinkedListVersionWideningTests.cpp, TheObjectGrewToFortyEightBytesOnLp64.
    if constexpr (sizeof(void*) == 8) {
        EXPECT_EQ(sizeof(G::List<int>), 40u);
        EXPECT_EQ(sizeof(G::HashSet<int>), 64u);
        EXPECT_EQ(sizeof(G::Dictionary<int, int>), 64u);
        EXPECT_EQ(sizeof(G::SortedDictionary<int, int>), 56u);
        EXPECT_EQ(sizeof(G::SortedList<int, int>), 56u);
        EXPECT_EQ(sizeof(G::OrderedDictionary<int, int>), 88u);
        EXPECT_EQ(sizeof(G::Queue<int>), 88u);
        EXPECT_EQ(sizeof(G::Stack<int>), 88u);
        EXPECT_EQ(sizeof(NG::ArrayList), 40u);
        EXPECT_EQ(sizeof(NG::Hashtable), 72u);
        EXPECT_EQ(sizeof(NG::ListDictionaryInternal), 40u);
        EXPECT_EQ(sizeof(NG::Queue), 96u);
        EXPECT_EQ(sizeof(NG::Stack), 96u);
        EXPECT_EQ(sizeof(NG::BitArray), 48u);
        EXPECT_EQ(alignof(G::List<int>), 8u);
        EXPECT_EQ(alignof(NG::BitArray), 8u);
    } else {
        SUCCEED() << "published sizes are LP64-only";
    }
}

TEST(CollectionVersionCounterCompatibility, PublishedIteratorSizesAreUnchanged) {
    if constexpr (sizeof(void*) == 8) {
        EXPECT_EQ(sizeof(G::HashSet<int>::iterator), 24u);
        EXPECT_EQ(sizeof(G::HashSet<int>::const_iterator), 24u);
        EXPECT_EQ(sizeof(G::Dictionary<int, int>::iterator), 24u);
        EXPECT_EQ(sizeof(G::SortedDictionary<int, int>::Iterator), 24u);
        EXPECT_EQ(sizeof(G::OrderedDictionary<int, int>::Iterator), 32u);
    } else {
        SUCCEED() << "published sizes are LP64-only";
    }
}

}  // namespace
