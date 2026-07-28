// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1792
// (REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST), design record
// docs/IEnumeratorCurrentSafetyDesign.md.
//
// #1792 is a DESIGN ticket: it changes no production behaviour. This suite
// therefore pins the behaviour that exists TODAY, in two deliberately separated
// groups, exactly as ticket #1790 did:
//
//   * EnumeratorCurrentContract   -- behaviour that must SURVIVE the future
//     implementation ticket #1793 unchanged: the before-start/after-end state
//     machine on both accessors, the fail-fast guard, and the fact that a
//     snapshot enumerator is unaffected by later collection mutation.
//
//   * EnumeratorCurrentDivergence -- the measured divergence from .NET that
//     #1793 exists to close. Every case asserts what this port does NOW, with
//     the .NET behaviour named in a comment, so #1793 must consciously FLIP it
//     rather than silently drift past it. The static_assert cases are the
//     load-bearing part: #1793 physically cannot land without editing them.
//     These are the cases the ticket's acceptance criteria require ("permanent
//     regressions must cover a write attempted through the non-generic
//     enumerator interface during enumeration, for at least one collection of
//     each storage family").
//
// Real .NET's IEnumerator.Current returns `object` BY VALUE -- a boxed copy for
// a value type -- captured into an enumerator-owned field at MoveNext
// (List.cs:1219 `_current = localList._items[_index]`, ArrayList.cs:2606,
// Dictionary.cs:1934). No .NET enumerator hands out a reference to live
// collection storage through the non-generic interface, and
// IEnumerator<out T>'s covariance makes it impossible for T to appear in an
// input position at all. This port's Generic/IEnumerator.hpp instead bridges
// with `return const_cast<T*>(&Current());`, publishing a MUTABLE void* that
// aliases live storage. See the design record for the alternatives analysis.
#include <gtest/gtest.h>

#include <any>
#include <memory>
#include <string>
#include <vector>

#include "System/InvalidOperationException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/Collections/ArrayList.hpp"
#include "System/Collections/Concurrent/ConcurrentQueue.hpp"
#include "System/Collections/Generic/IEnumerator.hpp"
#include "System/Collections/Generic/LinkedList.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/Generic/SortedList.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/ObjectModel/Collection.hpp"
#include "System/Collections/ObjectModel/ReadOnlyCollection.hpp"

namespace {

namespace G = System::Collections::Generic;
namespace OM = System::Collections::ObjectModel;

/** Owns the enumerator GetEnumerator() hands back, which is caller-owned. */
template <typename E>
class Owning {
    E* e_;
public:
    explicit Owning(E* e) : e_(e) {}
    Owning(const Owning&) = delete;
    Owning& operator=(const Owning&) = delete;
    ~Owning() { delete e_; }
    E* operator->() const { return e_; }
    E& operator*() const { return *e_; }
    [[nodiscard]] E* get() const { return e_; }
};

/** True when an outstanding fail-fast enumerator still accepts MoveNext(). */
template <typename E>
bool StillValid(E* e) {
    try {
        e->MoveNext();
        return true;
    } catch (const System::InvalidOperationException&) {
        return false;
    }
}

} // namespace

// ===========================================================================
// Contract -- must survive ticket #1793 unchanged.
// ===========================================================================

// Both accessors reject a read before the first MoveNext, with the exact .NET
// message (Strings.resx InvalidOperation_EnumNotStarted).
TEST(EnumeratorCurrentContract, BothAccessorsRejectReadBeforeFirstMoveNext) {
    G::List<int> list;
    list.Add(1);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());

    try {
        (void)e->Current();
        ADD_FAILURE() << "Expected System::InvalidOperationException";
    } catch (const System::InvalidOperationException& ex) {
        EXPECT_STREQ(ex.what(), "Enumeration has not started. Call MoveNext.");
    }
    EXPECT_THROW((void)e->getCurrentProperty(), System::InvalidOperationException);
}

// ... and after the last element (InvalidOperation_EnumEnded).
TEST(EnumeratorCurrentContract, BothAccessorsRejectReadAfterEnd) {
    G::List<int> list;
    list.Add(1);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    ASSERT_FALSE(e->MoveNext());

    try {
        (void)e->Current();
        ADD_FAILURE() << "Expected System::InvalidOperationException";
    } catch (const System::InvalidOperationException& ex) {
        EXPECT_STREQ(ex.what(), "Enumeration already finished.");
    }
    EXPECT_THROW((void)e->getCurrentProperty(), System::InvalidOperationException);
}

// Reset returns to the before-start state for BOTH accessors. The retained
// pointer hazard this leaves open is Divergence, below; the state machine
// itself is contract.
TEST(EnumeratorCurrentContract, ResetReturnsBothAccessorsToBeforeStart) {
    G::List<int> list;
    list.Add(1);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    ASSERT_NO_THROW((void)e->getCurrentProperty());

    e->Reset();
    EXPECT_THROW((void)e->Current(), System::InvalidOperationException);
    EXPECT_THROW((void)e->getCurrentProperty(), System::InvalidOperationException);
}

// The fail-fast guard itself works. This is the control that makes every
// Divergence case below evidence rather than noise.
TEST(EnumeratorCurrentContract, StructuralMutationStillInvalidatesTheEnumerator) {
    G::List<int> list;
    list.Add(1);
    list.Add(2);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    list.Add(3);
    EXPECT_FALSE(StillValid(e.get()));
    EXPECT_THROW(e->Reset(), System::InvalidOperationException);
}

// A snapshot enumerator is, correctly, unaffected by a later mutation of the
// collection it was taken from. #1793 must not change this.
TEST(EnumeratorCurrentContract, SnapshotEnumeratorIsIsolatedFromLaterMutation) {
    System::Collections::Concurrent::ConcurrentQueue<int> q;
    q.Enqueue(1);
    q.Enqueue(2);
    Owning<G::IEnumerator<int>> e(q.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    EXPECT_EQ(e->Current(), 1);

    int drained = 0;
    ASSERT_TRUE(q.TryDequeue(drained));
    EXPECT_TRUE(e->MoveNext());       // the snapshot walks on regardless
    EXPECT_EQ(e->Current(), 2);
}

// ReadOnlyCollection's declared read-only contract, through its own members.
// That the enumerator bypasses it is Divergence, below.
TEST(EnumeratorCurrentContract, ReadOnlyCollectionMembersRejectMutation) {
    auto backing = std::make_shared<std::vector<int>>(std::vector<int>{1, 2});
    OM::ReadOnlyCollection<int> ro(backing);

    EXPECT_THROW(ro[0] = 9, System::NotSupportedException);
    EXPECT_THROW(ro.Add(3), System::NotSupportedException);
    EXPECT_EQ(static_cast<const OM::ReadOnlyCollection<int>&>(ro)[0], 1);
}

// The non-generic Hashtable enumerator's typed accessors enforce the same
// state machine as Current.
TEST(EnumeratorCurrentContract, HashtableEnumeratorRejectsAccessorsBeforeStart) {
    System::Collections::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));
    Owning<System::Collections::IDictionaryEnumerator> e(
        static_cast<System::Collections::IDictionaryEnumerator*>(table.GetEnumerator()));

    EXPECT_THROW((void)e->getCurrentProperty(), System::InvalidOperationException);
    EXPECT_THROW((void)e->getEntryProperty(), System::InvalidOperationException);
    EXPECT_THROW((void)e->getKeyProperty(), System::InvalidOperationException);
    EXPECT_THROW((void)e->getValueProperty(), System::InvalidOperationException);
}

// The typed accessor is const-qualified on every implementation. #1793 keeps
// this; only the non-generic bridge changes.
TEST(EnumeratorCurrentContract, TypedCurrentIsConstQualified) {
    static_assert(std::is_same_v<decltype(std::declval<const G::IEnumerator<int>&>().Current()),
                                 const int&>,
                  "IEnumerator<T>::Current() must return const T&");
    static_assert(std::is_same_v<decltype(std::declval<const G::IEnumerator<std::string>&>().Current()),
                                 const std::string&>,
                  "IEnumerator<T>::Current() must return const T& for a non-trivial T");
    SUCCEED();
}

// ===========================================================================
// Divergence -- what #1793 exists to flip. Each case names .NET's behaviour.
// ===========================================================================

// LOAD-BEARING. Ticket #1793 cannot land without editing these two lines.
//
// .NET: `object IEnumerator.Current { get; }` -- a value, and for a value type
// a boxed COPY. There is no pointer, mutable or otherwise, anywhere in the
// managed contract.
TEST(EnumeratorCurrentDivergence, NonGenericAccessorStillReturnsAMutableVoidPointer) {
    static_assert(
        std::is_same_v<decltype(std::declval<const System::Collections::IEnumerator&>()
                                    .getCurrentProperty()),
                       void*>,
        "SR1792: the non-generic accessor still returns a MUTABLE void*. "
        "Ticket #1793 changes this to a by-value std::any; update this assertion "
        "and the Divergence cases below when it lands.");
    static_assert(
        std::is_same_v<decltype(std::declval<const G::IEnumerator<int>&>().getCurrentProperty()),
                       void*>,
        "SR1792: the generic bridge still returns a MUTABLE void*.");
    SUCCEED();
}

// Storage family 1: contiguous live vector storage (List<T>, and by the same
// bridge Queue<T>, Stack<T>, ObjectModel::Collection<T>).
//
// .NET: List<T>.Enumerator copies the element into its own `_current` field at
// MoveNext (List.cs:1219) and IEnumerator.Current returns that copy. A write
// through it is impossible, and List<T>'s own indexer setter bumps _version.
TEST(EnumeratorCurrentDivergence, VectorStorageIsAliasedAndWritableAndUntracked) {
    G::List<int> list;
    list.Add(10);
    list.Add(20);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    void* raw = e->getCurrentProperty();
    // The pointer aliases LIVE collection storage, not a copy.
    EXPECT_EQ(raw, static_cast<const void*>(&e->Current()));
    EXPECT_EQ(raw, static_cast<const void*>(&list.ToVector()[0]));

    *static_cast<int*>(raw) = 88;
    EXPECT_EQ(list.ToVector()[0], 88);            // the write landed in the collection
    EXPECT_TRUE(StillValid(e.get()));             // and the guard never fired
}

// Storage family 2: heap-allocated node storage.
TEST(EnumeratorCurrentDivergence, NodeStorageIsAliasedAndWritableAndUntracked) {
    G::LinkedList<int> list;
    list.AddLast(10);
    list.AddLast(20);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    *static_cast<int*>(e->getCurrentProperty()) = 55;
    EXPECT_EQ(list.getFirstProperty().getValueProperty(), 55);
    EXPECT_TRUE(StillValid(e.get()));
}

// Storage family 3: associative live storage (the value side of a map).
TEST(EnumeratorCurrentDivergence, AssociativeStorageValueIsAliasedAndWritableAndUntracked) {
    G::SortedList<int, int> list;
    list.Add(1, 100);
    list.Add(2, 200);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    *static_cast<int*>(e->getCurrentProperty()) = 44;
    EXPECT_EQ(list.GetValueAtIndex(0), 44);
    EXPECT_TRUE(StillValid(e.get()));
}

// Storage family 4: a type whose declared contract is READ-ONLY. The members
// pinned by the Contract suite above throw NotSupportedException; the
// enumerator hands out a writable pointer into the very same storage.
//
// .NET: ReadOnlyCollection<T>'s enumerator is the wrapped list's own
// enumerator, whose Current is a copy. No mutable path exists.
TEST(EnumeratorCurrentDivergence, ReadOnlyCollectionIsMutableThroughItsOwnEnumerator) {
    auto backing = std::make_shared<std::vector<int>>(std::vector<int>{1, 2});
    OM::ReadOnlyCollection<int> ro(backing);
    ASSERT_THROW(ro[0] = 9, System::NotSupportedException);

    Owning<G::IEnumerator<int>> e(ro.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    *static_cast<int*>(e->getCurrentProperty()) = 77;

    EXPECT_EQ(static_cast<const OM::ReadOnlyCollection<int>&>(ro)[0], 77);
    EXPECT_EQ((*backing)[0], 77);   // and the caller's shared backing store too
}

// Storage family 5: a non-generic collection whose elements are std::any. The
// pointer permits replacing not only the value but the element's dynamic TYPE.
//
// .NET: ArrayList's enumerator caches `_currentElement` (ArrayList.cs:2606)
// and its own indexer setter bumps _version (ArrayList.cs:126-131).
TEST(EnumeratorCurrentDivergence, ArrayListElementTypeIsRewritableAndUntracked) {
    System::Collections::ArrayList list;
    list.Add(std::any(10));
    list.Add(std::any(20));
    Owning<System::Collections::IEnumerator> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    auto* slot = static_cast<std::any*>(e->getCurrentProperty());
    EXPECT_EQ(slot, &list[0]);                      // aliases live storage
    *slot = std::any(std::string("retyped"));       // a new VALUE and a new TYPE
    EXPECT_EQ(list[0].type(), typeid(std::string));
    EXPECT_TRUE(StillValid(e.get()));
}

// Storage family 6: a live hash-map KEY. Only the aliasing is asserted here --
// actually rewriting the key breaks the container invariant, which is
// reproduced in the gitignored probe rather than performed in a permanent test.
//
// .NET: Hashtable's key collection enumerator returns the boxed key by value.
TEST(EnumeratorCurrentDivergence, HashtableKeyViewAliasesLiveKeyStorage) {
    System::Collections::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));

    std::unique_ptr<System::Collections::ICollection> keys(table.getKeysProperty());
    Owning<System::Collections::IEnumerator> e(keys->GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    void* raw = e->getCurrentProperty();
    const auto* asKey = static_cast<const std::string*>(raw);
    EXPECT_EQ(*asKey, "alpha");
    // The pointer is not const, and it aliases the key inside the live table.
    static_assert(std::is_same_v<decltype(e->getCurrentProperty()), void*>,
                  "SR1792: the key view still publishes a writable pointer to a live "
                  "std::unordered_map key.");
}

// Storage family 7: enumerator-owned snapshot storage. Here the same expression
// writes the enumerator's private copy and cannot reach the collection -- a
// materially different, milder defect that #1793 must not confuse with the
// others.
TEST(EnumeratorCurrentDivergence, SnapshotEnumeratorWriteHitsOnlyTheEnumeratorsCopy) {
    System::Collections::Concurrent::ConcurrentQueue<int> q;
    q.Enqueue(1);
    q.Enqueue(2);
    Owning<G::IEnumerator<int>> e(q.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    *static_cast<int*>(e->getCurrentProperty()) = 99;
    EXPECT_EQ(e->Current(), 99);        // the enumerator now reports 99 ...

    int peeked = 0;
    ASSERT_TRUE(q.TryPeek(peeked));
    EXPECT_EQ(peeked, 1);               // ... while the collection still holds 1
}

// The typed accessor and the non-generic one designate the SAME object at the
// same instant, one const and one not. This is the const-correctness breach in
// its smallest form.
TEST(EnumeratorCurrentDivergence, TypedAndNonGenericAccessorsDesignateTheSameObject) {
    G::List<int> list;
    list.Add(7);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    const int& readOnly = e->Current();
    void* writable = e->getCurrentProperty();
    ASSERT_EQ(static_cast<const void*>(&readOnly), writable);

    *static_cast<int*>(writable) = 42;
    EXPECT_EQ(readOnly, 42);            // observed through the const reference
}
