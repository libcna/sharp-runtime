// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for tickets #1792 (design,
// REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST) and #1793 (implementation,
// REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT), design record
// docs/IEnumeratorCurrentSafetyDesign.md.
//
// #1792 opened this file with two suites: an EnumeratorCurrentContract group
// pinning behaviour that had to survive #1793 unchanged, and an
// EnumeratorCurrentDivergence group asserting, case by case, the measured
// divergence from .NET that #1793 existed to close. #1793 FLIPPED the second
// group rather than deleting it -- every Divergence case is still here, under
// EnumeratorCurrentSafety, asserting the opposite outcome on the same
// collection through the same accessor, so the closed defect stays covered
// instead of merely disappearing from the suite. The static_asserts were the
// load-bearing part and are still load-bearing: they now pin `std::any`, so a
// revert to `void*` cannot land silently either.
//
// Real .NET's IEnumerator.Current returns `object` BY VALUE -- a boxed copy for
// a value type -- captured into an enumerator-owned field at MoveNext
// (List.cs:1219 `_current = localList._items[_index]`, ArrayList.cs:2606,
// Dictionary.cs:1934). No .NET enumerator hands out a reference to live
// collection storage through the non-generic interface, and
// IEnumerator<out T>'s covariance makes it impossible for T to appear in an
// input position at all. Before #1793 this port's Generic/IEnumerator.hpp
// bridged with `return const_cast<T*>(&Current());`, publishing a MUTABLE void*
// that aliased live storage; it now returns `std::any(Current())`.
#include <gtest/gtest.h>

#include <any>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "System/InvalidOperationException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/Collections/ArrayList.hpp"
#include "System/Collections/BitArray.hpp"
#include "System/Collections/Concurrent/ConcurrentQueue.hpp"
#include "System/Collections/Generic/IEnumerator.hpp"
#include "System/Collections/Generic/LinkedList.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/Generic/SortedList.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"
#include "System/Collections/ObjectModel/Collection.hpp"
#include "System/Collections/ObjectModel/ReadOnlyCollection.hpp"
#include "System/Collections/Queue.hpp"
#include "System/Collections/Stack.hpp"

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

// Reset returns to the before-start state for BOTH accessors. That the box
// already handed out survives Reset is Safety, below; the state machine itself
// is contract.
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
// Safety case below evidence rather than noise.
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
// That the enumerator no longer bypasses it is Safety, below.
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
// Safety -- the FLIPPED former Divergence suite. Every case below asserts, on
// the same collection and through the same accessor, the opposite of what
// ticket #1792 measured. The .NET behaviour each one now matches is named in
// the comment, exactly as the divergence was.
// ===========================================================================

namespace {

/** Ownership- and copy-counted element type, for the allocation/lifetime cases. */
struct Counted {
    static int live;
    static int copies;
    static int destroys;

    int value;

    explicit Counted(int v = 0) : value(v) { ++live; }
    Counted(const Counted& other) : value(other.value) { ++live; ++copies; }
    Counted& operator=(const Counted& other) { value = other.value; return *this; }
    ~Counted() { --live; ++destroys; }

    bool operator==(const Counted& other) const { return value == other.value; }
};

int Counted::live = 0;
int Counted::copies = 0;
int Counted::destroys = 0;

/** Resets the counters so each case measures only its own work. */
struct CountedScope {
    CountedScope() { Counted::live = 0; Counted::copies = 0; Counted::destroys = 0; }
};

/**
 * A move-only element type: the C++ analogue of .NET's `ref struct` element,
 * which cannot be boxed. Deliberately hand-written rather than taken from a
 * collection, because no collection in this repository instantiates one.
 */
struct MoveOnly {
    std::unique_ptr<int> owned;

    explicit MoveOnly(int v) : owned(std::make_unique<int>(v)) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;
    ~MoveOnly() = default;
};

static_assert(!std::is_copy_constructible_v<MoveOnly>,
              "MoveOnly must not be copy-constructible, or it proves nothing");

/** Hand-written IEnumerator<MoveOnly>: the specialization must stay instantiable. */
class MoveOnlyEnumerator : public G::IEnumerator<MoveOnly> {
    const std::vector<MoveOnly>& data_;
    int index_ = -1;

public:
    explicit MoveOnlyEnumerator(const std::vector<MoveOnly>& d) : data_(d) {}

    bool MoveNext() override { return ++index_ < static_cast<int>(data_.size()); }
    void Reset() override { index_ = -1; }

    [[nodiscard]] const MoveOnly& Current() const override {
        if (index_ < 0 || index_ >= static_cast<int>(data_.size()))
            throw System::InvalidOperationException("Enumeration has not started. Call MoveNext.");
        return data_[static_cast<std::size_t>(index_)];
    }
};

/**
 * Hand-written IEnumerator<std::any>, for the nested-box case. The element type
 * is itself a box, so the bridge's `std::any(Current())` selects std::any's COPY
 * constructor rather than its value-forwarding one.
 */
class AnyElementEnumerator : public G::IEnumerator<std::any> {
    const std::vector<std::any>& data_;
    int index_ = -1;

public:
    explicit AnyElementEnumerator(const std::vector<std::any>& d) : data_(d) {}

    bool MoveNext() override { return ++index_ < static_cast<int>(data_.size()); }
    void Reset() override { index_ = -1; }

    [[nodiscard]] const std::any& Current() const override {
        if (index_ < 0 || index_ >= static_cast<int>(data_.size()))
            throw System::InvalidOperationException("Enumeration has not started. Call MoveNext.");
        return data_[static_cast<std::size_t>(index_)];
    }
};

} // namespace

// ---------------------------------------------------------------------------
// The load-bearing signature assertions, flipped.
// ---------------------------------------------------------------------------

// WAS EnumeratorCurrentDivergence.NonGenericAccessorStillReturnsAMutableVoidPointer.
//
// .NET: `object IEnumerator.Current { get; }` -- a value, and for a value type a
// boxed COPY. There is no pointer, mutable or otherwise, anywhere in the managed
// contract. std::any is the direct C++ counterpart, so this port now matches.
TEST(EnumeratorCurrentSafety, BothAccessorsReturnAnOwningStdAnyByValue) {
    static_assert(
        std::is_same_v<decltype(std::declval<const System::Collections::IEnumerator&>()
                                    .getCurrentProperty()),
                       std::any>,
        "SR1793: the non-generic accessor must return std::any BY VALUE. A "
        "pointer return -- mutable or const -- reintroduces the aliasing, "
        "lifetime and type-safety defects ticket #1793 closed.");
    static_assert(
        std::is_same_v<decltype(std::declval<const G::IEnumerator<int>&>().getCurrentProperty()),
                       std::any>,
        "SR1793: the generic bridge must return std::any BY VALUE.");
    // Not a reference, not a pointer: the caller owns the result outright.
    static_assert(!std::is_reference_v<decltype(std::declval<const G::IEnumerator<int>&>()
                                                    .getCurrentProperty())>,
                  "SR1793: the box must be returned by value, never by reference.");
    static_assert(
        std::is_same_v<decltype(std::declval<const G::IEnumerator<int>&>().Current()), const int&>,
        "SR1793: the TYPED accessor is unchanged at const T&.");
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Storage families 1-7, flipped: the box is a copy, not a window.
// ---------------------------------------------------------------------------

// Storage family 1: contiguous live vector storage.
// WAS VectorStorageIsAliasedAndWritableAndUntracked.
//
// .NET: List<T>.Enumerator copies the element into `_current` at MoveNext
// (List.cs:1219) and IEnumerator.Current returns that copy.
TEST(EnumeratorCurrentSafety, VectorStorageIsCopiedOutAndNotAliased) {
    G::List<int> list;
    list.Add(10);
    list.Add(20);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    EXPECT_EQ(boxed.type(), typeid(int));
    EXPECT_EQ(std::any_cast<int>(boxed), 10);

    // The box holds a COPY: its address is not the element's address.
    EXPECT_NE(static_cast<const void*>(std::any_cast<const int>(&boxed)),
              static_cast<const void*>(&e->Current()));

    // Writing to the box cannot reach the collection.
    boxed = std::any(88);
    EXPECT_EQ(list.ToVector()[0], 10);
    EXPECT_EQ(e->Current(), 10);
    EXPECT_TRUE(StillValid(e.get()));   // and no mutation was recorded
}

// Storage family 2: heap-allocated node storage.
// WAS NodeStorageIsAliasedAndWritableAndUntracked.
TEST(EnumeratorCurrentSafety, NodeStorageIsCopiedOutAndNotAliased) {
    G::LinkedList<int> list;
    list.AddLast(10);
    list.AddLast(20);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    EXPECT_EQ(std::any_cast<int>(boxed), 10);
    *std::any_cast<int>(&boxed) = 55;

    EXPECT_EQ(list.getFirstProperty().getValueProperty(), 10);
    EXPECT_TRUE(StillValid(e.get()));
}

// Storage family 3: associative live storage (the value side of a map).
// WAS AssociativeStorageValueIsAliasedAndWritableAndUntracked.
TEST(EnumeratorCurrentSafety, AssociativeStorageValueIsCopiedOutAndNotAliased) {
    G::SortedList<int, int> list;
    list.Add(1, 100);
    list.Add(2, 200);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    EXPECT_EQ(std::any_cast<int>(boxed), 100);
    *std::any_cast<int>(&boxed) = 44;

    EXPECT_EQ(list.GetValueAtIndex(0), 100);
    EXPECT_TRUE(StillValid(e.get()));
}

// Storage family 4: a type whose declared contract is READ-ONLY. This is the
// single most serious functional consequence #1792 reproduced: the type's whole
// reason to exist was defeated through its own public enumeration path.
// WAS ReadOnlyCollectionIsMutableThroughItsOwnEnumerator.
//
// .NET: ReadOnlyCollection<T>'s enumerator is the wrapped list's own enumerator,
// whose Current is a copy. No mutable path exists.
TEST(EnumeratorCurrentSafety, ReadOnlyCollectionIsNoLongerMutableThroughItsOwnEnumerator) {
    auto backing = std::make_shared<std::vector<int>>(std::vector<int>{1, 2});
    OM::ReadOnlyCollection<int> ro(backing);
    ASSERT_THROW(ro[0] = 9, System::NotSupportedException);

    Owning<G::IEnumerator<int>> e(ro.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    *std::any_cast<int>(&boxed) = 77;
    boxed = std::any(std::string("retyped"));

    EXPECT_EQ(static_cast<const OM::ReadOnlyCollection<int>&>(ro)[0], 1);
    EXPECT_EQ((*backing)[0], 1);   // and the caller's shared backing store too
}

// Storage family 5: a non-generic collection whose elements are std::any. The
// element already IS a box, so the accessor returns a COPY of that box -- not a
// std::any wrapping a std::any. This is the nested-any behaviour, pinned.
// WAS ArrayListElementTypeIsRewritableAndUntracked.
//
// .NET: ArrayList's enumerator caches `_currentElement` (ArrayList.cs:2606) and
// its own indexer setter bumps _version (ArrayList.cs:126-131).
TEST(EnumeratorCurrentSafety, ArrayListElementTypeIsNoLongerRewritable) {
    System::Collections::ArrayList list;
    list.Add(std::any(10));
    list.Add(std::any(20));
    Owning<System::Collections::IEnumerator> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    // NOT nested: one any_cast, not two.
    EXPECT_EQ(boxed.type(), typeid(int));
    EXPECT_NE(boxed.type(), typeid(std::any));
    EXPECT_EQ(std::any_cast<int>(boxed), 10);

    boxed = std::any(std::string("retyped"));   // a new VALUE and a new TYPE

    EXPECT_EQ(list[0].type(), typeid(int));
    EXPECT_EQ(std::any_cast<int>(list[0]), 10);
    EXPECT_TRUE(StillValid(e.get()));
}

// Storage family 6: a live hash-map KEY. #1792 measured that a write through the
// returned pointer rewrote a std::unordered_map key in place and left the entry
// unreachable by both its old and its new name while Count still reported it --
// a broken container invariant, not merely a changed value. That write is now
// inexpressible: the accessor hands out a copy of the key string.
// WAS HashtableKeyViewAliasesLiveKeyStorage.
//
// .NET: Hashtable's key collection enumerator returns the boxed key by value.
TEST(EnumeratorCurrentSafety, HashtableKeyViewCopiesTheKeyOutAndCannotCorruptTheTable) {
    System::Collections::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));
    table.Add(std::string("beta"), std::any(2));

    std::unique_ptr<System::Collections::ICollection> keys(table.getKeysProperty());
    Owning<System::Collections::IEnumerator> e(keys->GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    ASSERT_EQ(boxed.type(), typeid(std::string));
    const std::string original = std::any_cast<std::string>(boxed);
    EXPECT_TRUE(original == "alpha" || original == "beta");

    // Rewriting the copy leaves the table's key space completely intact.
    *std::any_cast<std::string>(&boxed) = "CORRUPTED";

    EXPECT_EQ(table.getCountProperty(), 2);
    EXPECT_TRUE(table.ContainsKey("alpha"));
    EXPECT_TRUE(table.ContainsKey("beta"));
    EXPECT_FALSE(table.ContainsKey("CORRUPTED"));
    EXPECT_EQ(std::any_cast<int>(table.at("alpha")), 1);
    EXPECT_EQ(std::any_cast<int>(table.at("beta")), 2);
}

// The value view returns a COPY of the element's own std::any, not a nested one:
// the same unwrapping depth as ArrayList's, pinned so the two cannot drift apart.
TEST(EnumeratorCurrentSafety, HashtableValueViewReturnsTheElementBoxNotANestedOne) {
    System::Collections::Hashtable table;
    table.Add(std::string("solo"), std::any(42));

    std::unique_ptr<System::Collections::ICollection> values(table.getValuesProperty());
    Owning<System::Collections::IEnumerator> e(values->GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    EXPECT_EQ(boxed.type(), typeid(int));
    EXPECT_NE(boxed.type(), typeid(std::any));
    EXPECT_EQ(std::any_cast<int>(boxed), 42);

    boxed = std::any(std::string("clobbered"));
    EXPECT_EQ(std::any_cast<int>(table.at("solo")), 42);
}

// Storage family 7: enumerator-owned snapshot storage. #1792's counterexample --
// the write landed in the enumerator's own copy and could not reach the
// collection. It now cannot reach the enumerator's copy either.
// WAS SnapshotEnumeratorWriteHitsOnlyTheEnumeratorsCopy.
TEST(EnumeratorCurrentSafety, SnapshotEnumeratorAlsoCopiesOutOfItsOwnStorage) {
    System::Collections::Concurrent::ConcurrentQueue<int> q;
    q.Enqueue(1);
    q.Enqueue(2);
    Owning<G::IEnumerator<int>> e(q.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    *std::any_cast<int>(&boxed) = 99;

    EXPECT_EQ(e->Current(), 1);         // the enumerator's snapshot is untouched
    int peeked = 0;
    ASSERT_TRUE(q.TryPeek(peeked));
    EXPECT_EQ(peeked, 1);               // and so is the collection
}

// The const-correctness breach in its smallest form, flipped: the typed and the
// type-erased accessors no longer designate one object with opposite mutability.
// WAS TypedAndNonGenericAccessorsDesignateTheSameObject.
TEST(EnumeratorCurrentSafety, TypedAndNonGenericAccessorsNoLongerDesignateTheSameObject) {
    G::List<int> list;
    list.Add(7);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    const int& readOnly = e->Current();
    std::any boxed = e->getCurrentProperty();
    EXPECT_NE(static_cast<const void*>(&readOnly),
              static_cast<const void*>(std::any_cast<const int>(&boxed)));

    *std::any_cast<int>(&boxed) = 42;
    EXPECT_EQ(readOnly, 7);             // the element itself never moved
}

// ---------------------------------------------------------------------------
// Ownership and lifetime: the box outlives everything.
// ---------------------------------------------------------------------------

TEST(EnumeratorCurrentSafety, BoxSurvivesMoveNextResetAndASecondMoveNext) {
    G::List<std::string> list;
    list.Add("first");
    list.Add("second");
    Owning<G::IEnumerator<std::string>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();

    ASSERT_TRUE(e->MoveNext());
    EXPECT_EQ(std::any_cast<std::string>(boxed), "first");

    e->Reset();
    EXPECT_EQ(std::any_cast<std::string>(boxed), "first");

    ASSERT_TRUE(e->MoveNext());
    EXPECT_EQ(std::any_cast<std::string>(boxed), "first");
    EXPECT_EQ(e->Current(), "first");
}

TEST(EnumeratorCurrentSafety, BoxSurvivesCollectionReallocationAndClear) {
    G::List<std::string> list;
    list.Add(std::string(64, 'x'));
    Owning<G::IEnumerator<std::string>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    std::any boxed = e->getCurrentProperty();

    for (int i = 0; i < 64; ++i) list.Add("filler");   // forces reallocation
    EXPECT_EQ(std::any_cast<std::string>(boxed), std::string(64, 'x'));

    list.Clear();
    EXPECT_EQ(std::any_cast<std::string>(boxed), std::string(64, 'x'));
    EXPECT_EQ(std::any_cast<std::string>(boxed).size(), 64u);
}

TEST(EnumeratorCurrentSafety, BoxSurvivesEnumeratorAndCollectionDestruction) {
    std::any boxed;
    {
        G::List<std::string> list;
        list.Add("owned-by-the-box");
        {
            Owning<G::IEnumerator<std::string>> e(list.GetEnumerator());
            ASSERT_TRUE(e->MoveNext());
            boxed = e->getCurrentProperty();
        }                                   // enumerator destroyed
        EXPECT_EQ(std::any_cast<std::string>(boxed), "owned-by-the-box");
    }                                       // collection destroyed
    EXPECT_EQ(std::any_cast<std::string>(boxed), "owned-by-the-box");
}

// The same shape on a snapshot enumerator, whose storage dies WITH the
// enumerator -- the one #1792 reproduced as an ASan heap-use-after-free.
TEST(EnumeratorCurrentSafety, BoxSurvivesASnapshotEnumeratorsOwnDestruction) {
    std::any boxed;
    System::Collections::Concurrent::ConcurrentQueue<std::string> q;
    q.Enqueue(std::string(64, 'q'));
    {
        Owning<G::IEnumerator<std::string>> e(q.GetEnumerator());
        ASSERT_TRUE(e->MoveNext());
        boxed = e->getCurrentProperty();
    }
    EXPECT_EQ(std::any_cast<std::string>(boxed), std::string(64, 'q'));
}

TEST(EnumeratorCurrentSafety, RepeatedCallsAtOnePositionReturnIndependentValues) {
    G::List<std::string> list;
    list.Add("shared");
    Owning<G::IEnumerator<std::string>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any first = e->getCurrentProperty();
    std::any second = e->getCurrentProperty();

    EXPECT_NE(static_cast<const void*>(std::any_cast<const std::string>(&first)),
              static_cast<const void*>(std::any_cast<const std::string>(&second)));

    *std::any_cast<std::string>(&first) = "changed";
    EXPECT_EQ(std::any_cast<std::string>(second), "shared");
    EXPECT_EQ(e->Current(), "shared");
}

// ---------------------------------------------------------------------------
// Ownership counting for a non-trivial T: no leak, no double destroy.
// ---------------------------------------------------------------------------

TEST(EnumeratorCurrentSafety, NonTrivialElementIsCopiedExactlyOncePerReadAndBalanced) {
    CountedScope scope;
    {
        G::List<Counted> list;
        list.Add(Counted(7));
        const int afterFill = Counted::live;

        Owning<G::IEnumerator<Counted>> e(list.GetEnumerator());
        ASSERT_TRUE(e->MoveNext());

        const int copiesBefore = Counted::copies;
        {
            std::any boxed = e->getCurrentProperty();
            EXPECT_EQ(Counted::copies, copiesBefore + 1);   // exactly one copy
            EXPECT_EQ(Counted::live, afterFill + 1);        // exactly one extra live
            EXPECT_EQ(std::any_cast<Counted>(boxed).value, 7);
            std::any_cast<Counted&>(boxed).value = 4242;
            EXPECT_EQ(e->Current().value, 7);               // the element is untouched
        }
        EXPECT_EQ(Counted::live, afterFill);                // the box released its copy
    }
    EXPECT_EQ(Counted::live, 0);                            // and nothing leaked
}

// ---------------------------------------------------------------------------
// Value and reference-like element types.
// ---------------------------------------------------------------------------

// A shared_ptr element: std::any copies the HANDLE, not the pointee. Mutating
// *X through it is not mutating the collection -- exactly .NET's reference-type
// semantics, and normal C++ value semantics for the handle.
TEST(EnumeratorCurrentSafety, SharedPtrElementCopiesTheHandleNotThePointee) {
    auto shared = std::make_shared<int>(5);
    G::List<std::shared_ptr<int>> list;
    list.Add(shared);
    const long useCountInCollection = shared.use_count();

    Owning<G::IEnumerator<std::shared_ptr<int>>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    auto copy = std::any_cast<std::shared_ptr<int>>(boxed);
    EXPECT_GT(shared.use_count(), useCountInCollection);   // the handle was copied
    EXPECT_EQ(copy.get(), shared.get());                   // to the SAME object

    *copy = 99;                       // documented: this is not a collection mutation
    EXPECT_EQ(*shared, 99);
    EXPECT_TRUE(StillValid(e.get())); // and no mutation counter moved

    // Replacing the handle in the box cannot reach the collection.
    boxed = std::any(std::make_shared<int>(1));
    EXPECT_EQ(list.ToVector()[0].get(), shared.get());
}

// A raw-pointer element: the box holds the pointer VALUE.
TEST(EnumeratorCurrentSafety, RawPointerElementCopiesThePointerValue) {
    int target = 11;
    int other = 22;
    G::List<int*> list;
    list.Add(&target);

    Owning<G::IEnumerator<int*>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    EXPECT_EQ(boxed.type(), typeid(int*));
    EXPECT_EQ(std::any_cast<int*>(boxed), &target);

    *std::any_cast<int*>(&boxed) = &other;   // repoint the copy
    EXPECT_EQ(list.ToVector()[0], &target);  // the collection still points at target
}

// An element that is ITSELF a std::any, through the generic bridge. The bridge
// writes `std::any(Current())`, and std::any's copy constructor -- not its
// value-forwarding constructor -- wins, so the result is a COPY of the element's
// box, never a nested one. Pinned explicitly, because the opposite would be an
// easy accident that only shows up as a double any_cast at the call site.
TEST(EnumeratorCurrentSafety, StdAnyElementIsCopiedNotNested) {
    // Hand-written rather than taken from G::List<std::any>, which cannot be
    // instantiated at all: std::any is not equality-comparable, and List<T>'s
    // Contains/IndexOf need operator==. The bridge under test is the same one.
    std::vector<std::any> data{std::any(std::string("inner"))};
    AnyElementEnumerator e(data);
    ASSERT_TRUE(e.MoveNext());

    std::any boxed = e.getCurrentProperty();
    EXPECT_EQ(boxed.type(), typeid(std::string));
    EXPECT_NE(boxed.type(), typeid(std::any));
    EXPECT_EQ(std::any_cast<std::string>(boxed), "inner");

    boxed = std::any(0);
    EXPECT_EQ(std::any_cast<std::string>(data[0]), "inner");
    EXPECT_EQ(e.Current().type(), typeid(std::string));
}

// A DictionaryEntry element, from the Hashtable entry enumerator.
TEST(EnumeratorCurrentSafety, DictionaryEntryIsCopiedOutAndCannotCorruptTheTable) {
    System::Collections::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));
    Owning<System::Collections::IDictionaryEnumerator> e(
        static_cast<System::Collections::IDictionaryEnumerator*>(table.GetEnumerator()));
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    ASSERT_EQ(boxed.type(), typeid(System::Collections::DictionaryEntry));

    auto& entry = std::any_cast<System::Collections::DictionaryEntry&>(boxed);
    entry = System::Collections::DictionaryEntry(std::string("spoofed"), std::any(999));

    EXPECT_EQ(table.getCountProperty(), 1);
    EXPECT_TRUE(table.ContainsKey("alpha"));
    EXPECT_FALSE(table.ContainsKey("spoofed"));
    EXPECT_EQ(std::any_cast<int>(table.at("alpha")), 1);
    // The enumerator's own cache is unaffected too -- the pre-#1793 pointer to a
    // `mutable DictionaryEntry` let a caller desynchronise it from the table.
    EXPECT_EQ(std::any_cast<std::string>(
                  std::any_cast<System::Collections::DictionaryEntry>(e->getCurrentProperty())
                      .getKeyProperty()),
              "alpha");
}

// ---------------------------------------------------------------------------
// The move-only element type: the ONE new behaviour this ticket introduces.
// ---------------------------------------------------------------------------

// .NET's Generic/IEnumerator.cs comment: "An implementation of an enumerator
// using a ref struct T will not be able to implement IEnumerator.Current to
// return that T (as doing so would require boxing). It should throw a
// NotSupportedException from that property implementation."
TEST(EnumeratorCurrentSafety, MoveOnlyElementThrowsNotSupportedOnlyFromTheTypeErasedPath) {
    std::vector<MoveOnly> data;
    data.emplace_back(1);
    data.emplace_back(2);
    MoveOnlyEnumerator e(data);

    // The specialization is instantiable, and the typed path works.
    ASSERT_TRUE(e.MoveNext());
    EXPECT_EQ(*e.Current().owned, 1);

    // Only the type-erased path refuses -- at a valid position -- with the
    // exact selected message.
    try {
        (void)e.getCurrentProperty();
        ADD_FAILURE() << "Expected System::NotSupportedException";
    } catch (const System::NotSupportedException& ex) {
        EXPECT_STREQ(ex.what(),
                     "The element type cannot be boxed; use the typed Current() accessor.");
    }

    // MoveNext, Reset and Current all remain usable afterwards.
    ASSERT_TRUE(e.MoveNext());
    EXPECT_EQ(*e.Current().owned, 2);
    EXPECT_FALSE(e.MoveNext());
    e.Reset();
    ASSERT_TRUE(e.MoveNext());
    EXPECT_EQ(*e.Current().owned, 1);

    // Through a base pointer too -- the virtual dispatch reaches the same body.
    System::Collections::IEnumerator& base = e;
    EXPECT_THROW((void)base.getCurrentProperty(), System::NotSupportedException);
}

// The NotSupportedException path must not swallow the state machine: a
// before-start or after-end read still reports InvalidOperationException,
// because the bridge calls Current() first even on the non-copyable branch.
// This is the ordering guarantee of design section 18, and it is the one place
// where the design's section 14.2 sketch had to be corrected: an `if constexpr`
// whose else-branch merely throws discards the only call to Current(), which
// would have converted this pre-existing InvalidOperationException path into a
// NotSupportedException with no diagnostic. Caught by this test.
TEST(EnumeratorCurrentSafety, MoveOnlyStateMachineStillPrecedesTheBoxingRefusal) {
    std::vector<MoveOnly> data;
    data.emplace_back(1);
    MoveOnlyEnumerator e(data);

    EXPECT_THROW((void)e.getCurrentProperty(), System::InvalidOperationException);

    ASSERT_TRUE(e.MoveNext());
    EXPECT_THROW((void)e.getCurrentProperty(), System::NotSupportedException);

    ASSERT_FALSE(e.MoveNext());
    EXPECT_THROW((void)e.getCurrentProperty(), System::InvalidOperationException);

    e.Reset();
    EXPECT_THROW((void)e.getCurrentProperty(), System::InvalidOperationException);
}

// The copyable case must be unaffected by the constraint that produces it.
TEST(EnumeratorCurrentSafety, CopyableElementNeverTakesTheNotSupportedPath) {
    static_assert(std::is_copy_constructible_v<int>);
    static_assert(std::is_copy_constructible_v<std::string>);
    static_assert(std::is_copy_constructible_v<Counted>);

    G::List<int> list;
    list.Add(1);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    EXPECT_NO_THROW((void)e->getCurrentProperty());
}

// ---------------------------------------------------------------------------
// Type safety: the box carries its type, and a wrong cast is diagnosed.
// ---------------------------------------------------------------------------

// #1792 measured that a same-width wrong cast through the void* produced a
// silently wrong value with NO diagnostic from any sanitizer. std::any_cast
// throws instead.
TEST(EnumeratorCurrentSafety, WrongAnyCastThrowsInsteadOfReinterpretingBytes) {
    G::List<int> list;
    list.Add(0x41424344);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    EXPECT_EQ(std::any_cast<int>(boxed), 0x41424344);
    // float is the same width as int -- the exact case that used to be silent.
    EXPECT_THROW((void)std::any_cast<float>(boxed), std::bad_any_cast);
    EXPECT_THROW((void)std::any_cast<std::string>(boxed), std::bad_any_cast);
    // The pointer form reports the mismatch without throwing.
    EXPECT_EQ(std::any_cast<float>(&boxed), nullptr);
    EXPECT_NE(std::any_cast<int>(&boxed), nullptr);
}

// std::bad_any_cast is a `std::` exception, not a `System::` one. Consistent
// with how this port already exposes std::any (ArrayList, DictionaryEntry,
// ticket #1771's CopyTo), and documented rather than wrapped.
TEST(EnumeratorCurrentSafety, WrongCastThrowsAStdExceptionNotASystemOne) {
    G::List<int> list;
    list.Add(1);
    Owning<G::IEnumerator<int>> e(list.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    std::any boxed = e->getCurrentProperty();

    try {
        (void)std::any_cast<double>(boxed);
        ADD_FAILURE() << "Expected std::bad_any_cast";
    } catch (const System::Exception&) {
        ADD_FAILURE() << "std::bad_any_cast must not be a System::Exception";
    } catch (const std::bad_any_cast&) {
        SUCCEED();
    }
}

// ---------------------------------------------------------------------------
// Mutation-counter behaviour: reading is a read, and stays one.
// ---------------------------------------------------------------------------

// Observed through the public fail-fast guard rather than a private field: an
// INDEPENDENT enumerator created before the copy-out is the strongest available
// witness that no mutation was recorded, because it is exactly what a consumer
// experiences. The control at the end proves the guard still fires.
TEST(EnumeratorCurrentSafety, CopyingOutNeverAdvancesTheMutationCounter) {
    G::List<int> list;
    list.Add(1);
    list.Add(2);
    list.Add(3);

    Owning<G::IEnumerator<int>> walker(list.GetEnumerator());
    ASSERT_TRUE(walker->MoveNext());
    Owning<G::IEnumerator<int>> observer(list.GetEnumerator());

    for (int i = 0; i < 16; ++i) {
        std::any boxed = walker->getCurrentProperty();
        *std::any_cast<int>(&boxed) = 1000 + i;
        boxed = std::any(std::string("retyped"));
    }

    EXPECT_TRUE(StillValid(observer.get()));   // the independent observer is intact
    EXPECT_EQ(list.ToVector(), (std::vector<int>{1, 2, 3}));

    list.Add(4);                               // control: a REAL mutation
    EXPECT_FALSE(StillValid(observer.get()));
}

// ---------------------------------------------------------------------------
// Every remaining non-generic implementation family.
// ---------------------------------------------------------------------------

// The two collections whose element type IS a void*: nothing was ever aliased
// here, and the box holds the stored pointer by value.
TEST(EnumeratorCurrentSafety, NonGenericStackAndQueueBoxTheStoredPointerByValue) {
    int a = 1, b = 2;

    System::Collections::Stack stack;
    stack.Push(&a);
    stack.Push(&b);
    Owning<System::Collections::IEnumerator> se(stack.GetEnumerator());
    ASSERT_TRUE(se->MoveNext());
    std::any sBoxed = se->getCurrentProperty();
    EXPECT_EQ(sBoxed.type(), typeid(void*));
    EXPECT_EQ(std::any_cast<void*>(sBoxed), &b);   // top of the stack

    System::Collections::Queue queue;
    queue.Enqueue(&a);
    queue.Enqueue(&b);
    Owning<System::Collections::IEnumerator> qe(queue.GetEnumerator());
    ASSERT_TRUE(qe->MoveNext());
    EXPECT_EQ(std::any_cast<void*>(qe->getCurrentProperty()), &a);   // FIFO
}

// BitArray's enumerator pointed at a `mutable bool` cache, through which a
// caller could desynchronise the enumerator from the array it was walking.
TEST(EnumeratorCurrentSafety, BitArrayBoxesTheBitAndCannotDesynchroniseTheEnumerator) {
    System::Collections::BitArray bits(2, false);
    bits.Set(0, true);
    Owning<System::Collections::IEnumerator> e(bits.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    EXPECT_EQ(boxed.type(), typeid(bool));
    EXPECT_TRUE(std::any_cast<bool>(boxed));

    *std::any_cast<bool>(&boxed) = false;
    EXPECT_TRUE(bits.Get(0));                                   // the array is intact
    EXPECT_TRUE(std::any_cast<bool>(e->getCurrentProperty()));  // and so is the cache
}

// ListDictionaryInternal's two enumerators const_cast'd the CALLER's own
// `const void*` key. The box preserves the const, so recovering it yields a
// `const void*` and there is no write path at all.
TEST(EnumeratorCurrentSafety, ListDictionaryInternalPreservesTheConstOnItsBoxedKey) {
    System::Collections::ListDictionaryInternal dictionary;
    const int key = 1;
    int value = 99;
    dictionary.Add(&key, &value);

    Owning<System::Collections::IDictionaryEnumerator> entries(dictionary.GetEnumerator());
    ASSERT_TRUE(entries->MoveNext());
    std::any boxedKey = entries->getCurrentProperty();
    EXPECT_EQ(boxedKey.type(), typeid(const void*));
    EXPECT_EQ(std::any_cast<const void*>(boxedKey), &key);
    EXPECT_EQ(std::any_cast<void*>(&boxedKey), nullptr);   // no non-const view exists

    std::unique_ptr<System::Collections::ICollection> keys(dictionary.getKeysProperty());
    Owning<System::Collections::IEnumerator> keyWalk(keys->GetEnumerator());
    ASSERT_TRUE(keyWalk->MoveNext());
    EXPECT_EQ(std::any_cast<const void*>(keyWalk->getCurrentProperty()), &key);

    std::unique_ptr<System::Collections::ICollection> values(dictionary.getValuesProperty());
    Owning<System::Collections::IEnumerator> valueWalk(values->GetEnumerator());
    ASSERT_TRUE(valueWalk->MoveNext());
    EXPECT_EQ(std::any_cast<const void*>(valueWalk->getCurrentProperty()), &value);

    EXPECT_EQ(dictionary.getCountProperty(), 1);
    EXPECT_TRUE(dictionary.Contains(&key));
}

// ObjectModel::Collection<T> has NO mutation counter and no fail-fast guard at
// all, which is why the pre-#1793 writable pointer was worst here: nothing could
// even have detected the write. #1793 removes the write path rather than adding
// a counter -- adding one is ticket #1791 Phase 2's business.
TEST(EnumeratorCurrentSafety, ObjectModelCollectionIsCopiedOutDespiteHavingNoCounter) {
    OM::Collection<int> collection;
    collection.Add(33);
    Owning<G::IEnumerator<int>> e(collection.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    *std::any_cast<int>(&boxed) = 44;
    EXPECT_EQ(collection[0], 33);
}
