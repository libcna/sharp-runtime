// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1798
// (REMED-COLL-LISTDICTINTERNAL-PARITY), whose design record is
// docs/ListDictionaryInternalSetterDesign.md (ticket #1799). No SR-AUD-*
// identifier: the audit numbering is frozen at 364 and all six defects were
// found during remediation.
//
// Six defects were measured against the committed headers before anything
// changed, and every one of them is pinned shut below:
//
//   D1  setItem's REPLACE branch returned before ++version_, so a replacement
//       mutated the dictionary with the fail-fast counter unmoved (3 -> 3).
//   D2  Four outstanding enumerator kinds therefore walked to the end after a
//       replacement without throwing -- the dictionary's IDictionaryEnumerator,
//       the key view, the value view, and the same reached through an
//       IDictionary& -- and the VALUE VIEW enumerated the post-mutation value,
//       with zero AddressSanitizer and zero UndefinedBehaviorSanitizer reports.
//   D3  All five raw-key entry points accepted nullptr, and setItem STORED it,
//       where .NET begins each with ArgumentNullException.ThrowIfNull(key) and
//       where this port's Hashtable has rejected null since ticket #1775.
//   D5  MemberCollection::copyToCore boxed const_cast<void*>(n.key) where all
//       four other key surfaces box const void*, so one view had two
//       incompatible element types and a write through the writable pointer it
//       manufactured for a key the caller declared const was an
//       AddressSanitizer SEGV on read-only storage.
//   D7  The two IDictionary implementations disagreed on every null-key row and
//       on the replace-version row, so no polymorphic consumer could rely on
//       either contract. The interface-level cases here are therefore
//       PARAMETERISED over both implementations rather than written against one.
//   D9  An equal-value replacement did not bump either. It must: equality of a
//       void* is address equality, and .NET compares neither value.
//
// Two DELIBERATE deviations from .NET ListDictionaryInternal are asserted here
// as contract, not tolerated as accident. .NET's indexer setter, Add and Remove
// all do `version++` FIRST and UNCONDITIONALLY, before they even search, so a
// duplicate-key Add that throws and a Remove of an absent key both invalidate
// every outstanding enumerator there. .NET's own Hashtable does neither, so
// "match .NET" is not by itself a specification. This port advances the counter
// on EFFECTIVE MUTATION ONLY -- which matches .NET on every row where .NET's two
// implementations agree, takes the Hashtable rule where they disagree, matches
// what detail/MutationCounter.hpp already documents, and avoids manufacturing
// two new false-positive InvalidOperationExceptions out of calls that changed
// nothing. Clear is the single carve-out and keeps its unconditional bump,
// matching .NET ListDictionaryInternal.
#include <gtest/gtest.h>

#include <any>
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Collections/DictionaryEntry.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"
#include "System/Collections/detail/MutationCounter.hpp"

using SharpRuntime::intcs;
using System::Collections::DictionaryEntry;
using System::Collections::Hashtable;
using System::Collections::ICollection;
using System::Collections::IDictionary;
using System::Collections::IDictionaryEnumerator;
using System::Collections::IEnumerator;
using System::Collections::ListDictionaryInternal;

// ---------------------------------------------------------------------------
// Mutation-counter seam. Declared by detail/MutationCounter.hpp and befriended
// by both dictionaries; never defined in production. Reading the counter
// DIRECTLY is what lets "a replacement bumps exactly once" and "a rejected null
// key moves nothing" be assertions rather than inferences from an enumerator's
// silence -- and "exactly once" is not observable through fail-fast alone.
//
// Spelled token-for-token as DictionaryEnumeratorKeyValueSafetyTests.cpp and
// HashtableValueAccessSafetyTests.cpp spell it, deliberately: these
// specialisations share a binary, so a divergent definition of the same
// explicit specialisation would be an ODR violation. That divergence already
// exists between those two files and CollectionVersionCounterTests.cpp, which
// adds a positionVersion member; it is PRE-EXISTING, is tracked as inactive
// ticket #1800, is NOT introduced or widened here, and is NOT fixed here.
// ---------------------------------------------------------------------------
namespace SharpRuntime::Testing {

template<typename V>
struct CollectionVersionAccess<System::Collections::detail::BasicMutationCounter<V>> {
    using Counter = System::Collections::detail::BasicMutationCounter<V>;
    static V read(const Counter& c) { return c.getValueProperty(); }
};

#define SR1794_SEAM_BODY(...)                                                              \
    using Coll = __VA_ARGS__;                                                              \
    using Counter = std::remove_cvref_t<decltype(std::declval<Coll&>().version_)>;         \
    using Seam = CollectionVersionAccess<Counter>;                                         \
    using value_type = typename Counter::value_type;                                       \
    static value_type version(const Coll& c) { return Seam::read(c.version_); }

template<>
struct CollectionVersionAccess<System::Collections::Hashtable> {
    SR1794_SEAM_BODY(System::Collections::Hashtable)
};
template<>
struct CollectionVersionAccess<System::Collections::ListDictionaryInternal> {
    SR1794_SEAM_BODY(System::Collections::ListDictionaryInternal)
};

} // namespace SharpRuntime::Testing

namespace {

template<typename C>
auto versionOf(const C& c) {
    return SharpRuntime::Testing::CollectionVersionAccess<C>::version(c);
}

/**
 * Stable key/value storage. This dictionary borrows every pointer it is handed
 * and owns nothing, so the addresses must outlive it; std::deque never
 * invalidates references to existing elements when it grows.
 */
int* slot(int i) {
    static std::deque<int> storage;
    while (static_cast<int>(storage.size()) <= i) storage.push_back(i);
    return &storage[static_cast<std::size_t>(i)];
}

/** @brief Owning handle for the raw enumerator pointers this module hands out. */
template<typename T>
using Owning = std::unique_ptr<T>;

// ===========================================================================
// 1. Versioning -- the effective-mutation rule, on ListDictionaryInternal
// ===========================================================================

TEST(ListDictionarySetterVersioning, SetItemInsertBumpsExactlyOnce) {
    ListDictionaryInternal d;
    const auto before = versionOf(d);
    d.setItem(slot(0), slot(1));
    EXPECT_EQ(versionOf(d), before + 1);
    EXPECT_EQ(d.getCountProperty(), 1);
}

// D1 and D9: the defect this ticket exists to close. Before #1798 the replace
// branch returned before ++version_, so this read `before`, not `before + 1`.
TEST(ListDictionarySetterVersioning, SetItemReplaceBumpsExactlyOnce) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    const auto before = versionOf(d);
    d.setItem(slot(0), slot(2));
    EXPECT_EQ(versionOf(d), before + 1);
    EXPECT_EQ(d.getCountProperty(), 1) << "a replacement does not change Count";
    EXPECT_EQ(std::any_cast<void*>(d.getItem(slot(0))), slot(2));
}

// The value is never compared: equality of a void* is address equality, and
// .NET compares neither. Making the bump depend on it would silently make the
// contract depend on the caller's aliasing.
TEST(ListDictionarySetterVersioning, SetItemEqualValueReplaceStillBumps) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    const auto before = versionOf(d);
    d.setItem(slot(0), slot(1));   // the same value pointer, again
    EXPECT_EQ(versionOf(d), before + 1);
    EXPECT_EQ(d.getCountProperty(), 1);
}

TEST(ListDictionarySetterVersioning, SetItemReplacePreservesTheKeyAndDoesNotRelink) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    d.Add(slot(2), slot(3));
    d.setItem(slot(0), slot(4));

    // Order is unchanged: the node was updated in place, not erased and re-added.
    Owning<IDictionaryEnumerator> walk(d.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    EXPECT_EQ(std::any_cast<const void*>(walk->getKeyProperty()), slot(0));
    EXPECT_EQ(std::any_cast<void*>(walk->getValueProperty()), slot(4));
    ASSERT_TRUE(walk->MoveNext());
    EXPECT_EQ(std::any_cast<const void*>(walk->getKeyProperty()), slot(2));
    EXPECT_FALSE(walk->MoveNext());
}

TEST(ListDictionarySetterVersioning, AddNewKeyBumpsExactlyOnce) {
    ListDictionaryInternal d;
    const auto before = versionOf(d);
    d.Add(slot(0), slot(1));
    EXPECT_EQ(versionOf(d), before + 1);
}

// DELIBERATE DEVIATION from .NET ListDictionaryInternal, which bumps first and
// would invalidate every enumerator on a call that changed nothing.
TEST(ListDictionarySetterVersioning, AddOnADuplicateKeyThrowsAndDoesNotBump) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    const auto before = versionOf(d);

    EXPECT_THROW(d.Add(slot(0), slot(2)), System::ArgumentException);

    EXPECT_EQ(versionOf(d), before) << "a throwing Add is not an effective mutation";
    EXPECT_EQ(d.getCountProperty(), 1);
    EXPECT_EQ(std::any_cast<void*>(d.getItem(slot(0))), slot(1))
        << "the rejected value must not have been stored";
}

TEST(ListDictionarySetterVersioning, RemoveOfAPresentKeyBumpsExactlyOnce) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    const auto before = versionOf(d);
    d.Remove(slot(0));
    EXPECT_EQ(versionOf(d), before + 1);
    EXPECT_EQ(d.getCountProperty(), 0);
}

// Second DELIBERATE DEVIATION from .NET ListDictionaryInternal. This is also
// the row CollectionVersionCounterTests.cpp's ListDictionaryAdapter already
// asserts through kHasNoOpMutation = true, which literal .NET parity would have
// contradicted.
TEST(ListDictionarySetterVersioning, RemoveOfAnAbsentKeyDoesNotBump) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    const auto before = versionOf(d);
    d.Remove(slot(9));
    EXPECT_EQ(versionOf(d), before);
    EXPECT_EQ(d.getCountProperty(), 1);
}

// The single carve-out from "no bump for a no-op", matching .NET
// ListDictionaryInternal.Clear(), which bumps unconditionally. (.NET Hashtable
// early-returns instead; the two .NET implementations disagree on this row.)
TEST(ListDictionarySetterVersioning, ClearBumpsEvenWhenAlreadyEmpty) {
    ListDictionaryInternal d;
    const auto before = versionOf(d);
    d.Clear();
    EXPECT_EQ(versionOf(d), before + 1) << ".NET ListDictionaryInternal.Clear bumps unconditionally";

    d.Add(slot(0), slot(1));
    const auto beforeNonEmpty = versionOf(d);
    d.Clear();
    EXPECT_EQ(versionOf(d), beforeNonEmpty + 1);
    EXPECT_EQ(d.getCountProperty(), 0);
}

TEST(ListDictionarySetterVersioning, ReadsNeverBump) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    const auto before = versionOf(d);

    (void)d.getItem(slot(0));
    (void)d.getItem(slot(9));
    (void)d.Contains(slot(0));
    (void)d.Contains(slot(9));
    (void)d.getCountProperty();
    Owning<ICollection> keys(d.getKeysProperty());
    Owning<ICollection> values(d.getValuesProperty());
    (void)keys->getCountProperty();
    (void)values->getCountProperty();

    EXPECT_EQ(versionOf(d), before);
}

// ===========================================================================
// 2. Enumerator invalidation, all four kinds, both implementations
// ===========================================================================

enum class Implementation { Hashtable, ListDictionaryInternal };

const char* implementationName(const ::testing::TestParamInfo<Implementation>& info) {
    return info.param == Implementation::Hashtable ? "Hashtable" : "ListDictionaryInternal";
}

/**
 * One uniform way to seed either non-generic dictionary and then REPLACE the
 * value of an existing key, whose key spaces differ: Hashtable keys are strings
 * derived from an address, ListDictionaryInternal keys are the raw address.
 * Everything below drives the dictionary through IDictionary&, because the
 * contract being pinned belongs to the interface, not to either type.
 */
class ReplaceHarness {
public:
    virtual ~ReplaceHarness() = default;
    virtual IDictionary& dictionary() = 0;
    /** @brief Seeds two entries under keys this harness can later replace. */
    virtual void seed() = 0;
    /** @brief Replaces the first seeded key's value with a different one. */
    virtual void replaceFirstValue() = 0;
    /** @brief Replaces the first seeded key's value with the SAME one. */
    virtual void replaceFirstValueWithAnEqualValue() = 0;
};

class HashtableReplaceHarness : public ReplaceHarness {
    Hashtable table_;
    std::any first_ = std::any(1);
    std::any second_ = std::any(2);
    std::any replacement_ = std::any(3);
    int firstKey_ = 0;
    int secondKey_ = 0;

public:
    IDictionary& dictionary() override { return table_; }
    void seed() override {
        table_.Add(static_cast<const void*>(&firstKey_), &first_);
        table_.Add(static_cast<const void*>(&secondKey_), &second_);
    }
    void replaceFirstValue() override {
        table_.setItem(static_cast<const void*>(&firstKey_), &replacement_);
    }
    void replaceFirstValueWithAnEqualValue() override {
        table_.setItem(static_cast<const void*>(&firstKey_), &first_);
    }
};

class ListDictionaryReplaceHarness : public ReplaceHarness {
    ListDictionaryInternal dictionary_;
    std::deque<int> storage_;

public:
    IDictionary& dictionary() override { return dictionary_; }
    void seed() override {
        storage_ = {10, 20, 30, 40, 99};
        dictionary_.Add(&storage_[0], &storage_[2]);
        dictionary_.Add(&storage_[1], &storage_[3]);
    }
    void replaceFirstValue() override { dictionary_.setItem(&storage_[0], &storage_[4]); }
    void replaceFirstValueWithAnEqualValue() override {
        dictionary_.setItem(&storage_[0], &storage_[2]);
    }
};

std::unique_ptr<ReplaceHarness> makeReplaceHarness(Implementation which) {
    if (which == Implementation::Hashtable) return std::make_unique<HashtableReplaceHarness>();
    return std::make_unique<ListDictionaryReplaceHarness>();
}

class DictionaryReplacementInvalidation : public ::testing::TestWithParam<Implementation> {
protected:
    void SetUp() override {
        harness_ = makeReplaceHarness(GetParam());
        harness_->seed();
    }
    IDictionary& dictionary() { return harness_->dictionary(); }
    void replaceFirstValue() { harness_->replaceFirstValue(); }
    void replaceFirstValueWithAnEqualValue() {
        harness_->replaceFirstValueWithAnEqualValue();
    }

private:
    std::unique_ptr<ReplaceHarness> harness_;
};

INSTANTIATE_TEST_SUITE_P(
    AllNonGenericDictionaries, DictionaryReplacementInvalidation,
    ::testing::Values(Implementation::Hashtable, Implementation::ListDictionaryInternal),
    implementationName);

TEST_P(DictionaryReplacementInvalidation, TheDictionaryEnumeratorFailsFast) {
    Owning<IDictionaryEnumerator> walk(dictionary().GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    replaceFirstValue();
    EXPECT_THROW((void)walk->MoveNext(), System::InvalidOperationException);
}

TEST_P(DictionaryReplacementInvalidation, TheKeyViewEnumeratorFailsFast) {
    Owning<ICollection> keys(dictionary().getKeysProperty());
    Owning<IEnumerator> walk(keys->GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    replaceFirstValue();
    EXPECT_THROW((void)walk->MoveNext(), System::InvalidOperationException);
}

// The sharp one: the value view is the surface whose whole content is the thing
// that was replaced, and before #1798 it enumerated the new data silently.
TEST_P(DictionaryReplacementInvalidation, TheValueViewEnumeratorFailsFast) {
    Owning<ICollection> values(dictionary().getValuesProperty());
    Owning<IEnumerator> walk(values->GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    replaceFirstValue();
    EXPECT_THROW((void)walk->MoveNext(), System::InvalidOperationException);
}

TEST_P(DictionaryReplacementInvalidation, ThroughAnIDictionaryBaseReferenceItFailsFast) {
    IDictionary& asInterface = dictionary();
    Owning<IDictionaryEnumerator> walk(asInterface.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    replaceFirstValue();
    EXPECT_THROW((void)walk->MoveNext(), System::InvalidOperationException);
}

TEST_P(DictionaryReplacementInvalidation, AnEqualValueReplacementInvalidatesToo) {
    Owning<IDictionaryEnumerator> walk(dictionary().GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    replaceFirstValueWithAnEqualValue();
    EXPECT_THROW((void)walk->MoveNext(), System::InvalidOperationException);
}

TEST_P(DictionaryReplacementInvalidation, ResetAlsoFailsFastAfterAReplacement) {
    Owning<IDictionaryEnumerator> walk(dictionary().GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    replaceFirstValue();
    EXPECT_THROW(walk->Reset(), System::InvalidOperationException);
}

// A fresh enumerator taken after the replacement is of course valid: fail-fast
// is about enumerators that were outstanding ACROSS the mutation.
TEST_P(DictionaryReplacementInvalidation, AnEnumeratorTakenAfterTheReplacementIsValid) {
    replaceFirstValue();
    Owning<IDictionaryEnumerator> walk(dictionary().GetEnumerator());
    int visited = 0;
    while (walk->MoveNext()) ++visited;
    EXPECT_EQ(visited, 2);
}

// ===========================================================================
// 3. No-op and failed operations leave enumerators alone
// ===========================================================================

TEST(ListDictionaryNonMutation, AThrowingDuplicateAddLeavesAnOutstandingEnumeratorValid) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    d.Add(slot(2), slot(3));
    Owning<IDictionaryEnumerator> walk(d.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());

    EXPECT_THROW(d.Add(slot(0), slot(4)), System::ArgumentException);

    EXPECT_NO_THROW((void)walk->MoveNext());
}

TEST(ListDictionaryNonMutation, AnAbsentRemoveLeavesAnOutstandingEnumeratorValid) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    d.Add(slot(2), slot(3));
    Owning<IDictionaryEnumerator> walk(d.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());

    d.Remove(slot(9));

    EXPECT_NO_THROW((void)walk->MoveNext());
}

TEST(ListDictionaryNonMutation, ARejectedNullKeyLeavesAnOutstandingEnumeratorValid) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    d.Add(slot(2), slot(3));
    Owning<IDictionaryEnumerator> walk(d.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    const auto before = versionOf(d);

    EXPECT_THROW(d.Add(nullptr, slot(4)), System::ArgumentNullException);
    EXPECT_THROW(d.setItem(nullptr, slot(4)), System::ArgumentNullException);
    EXPECT_THROW((void)d.getItem(nullptr), System::ArgumentNullException);
    EXPECT_THROW((void)d.Contains(nullptr), System::ArgumentNullException);
    EXPECT_THROW(d.Remove(nullptr), System::ArgumentNullException);

    EXPECT_EQ(versionOf(d), before);
    EXPECT_NO_THROW((void)walk->MoveNext());
}

TEST(ListDictionaryNonMutation, ReadsLeaveAnOutstandingEnumeratorValid) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    d.Add(slot(2), slot(3));
    Owning<IDictionaryEnumerator> walk(d.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());

    (void)d.getItem(slot(0));
    (void)d.Contains(slot(2));
    (void)d.getCountProperty();

    EXPECT_NO_THROW((void)walk->MoveNext());
}

// ===========================================================================
// 4. Null-key rejection: exception type, parameter, HResult, message, rollback
// ===========================================================================

void expectExactNullKeyException(const std::function<void()>& call) {
    try {
        call();
        ADD_FAILURE() << "a null key must be rejected";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "key");
        EXPECT_STREQ(e.what(), "Value cannot be null. (Parameter 'key')");
        // E_POINTER / COR_E_ARGUMENTNULL, the same value .NET's
        // ArgumentNullException carries and the same one Hashtable already
        // produced before this ticket. Stored as a signed 32-bit value.
        EXPECT_EQ(e.getHResultProperty(), static_cast<intcs>(0x80004003));
    } catch (const std::exception& e) {
        ADD_FAILURE() << "expected System::ArgumentNullException, got: " << e.what();
    }
}

TEST(ListDictionaryNullKey, EveryRawKeyEntryPointRejectsNullWithTheExactContract) {
    ListDictionaryInternal d;
    expectExactNullKeyException([&] { d.Add(nullptr, slot(1)); });
    expectExactNullKeyException([&] { d.setItem(nullptr, slot(1)); });
    expectExactNullKeyException([&] { (void)d.getItem(nullptr); });
    expectExactNullKeyException([&] { (void)d.Contains(nullptr); });
    expectExactNullKeyException([&] { d.Remove(nullptr); });
    EXPECT_EQ(d.getCountProperty(), 0) << "and none of them inserted anything";
}

TEST(ListDictionaryNullKey, TheSameContractHoldsThroughAnIDictionaryBaseReference) {
    ListDictionaryInternal d;
    IDictionary& asInterface = d;
    expectExactNullKeyException([&] { asInterface.Add(nullptr, slot(1)); });
    expectExactNullKeyException([&] { asInterface.setItem(nullptr, slot(1)); });
    expectExactNullKeyException([&] { (void)asInterface.getItem(nullptr); });
    expectExactNullKeyException([&] { (void)asInterface.Contains(nullptr); });
    expectExactNullKeyException([&] { asInterface.Remove(nullptr); });
    EXPECT_EQ(asInterface.getCountProperty(), 0);
}

// setItem's null-key rejection must precede the LOOKUP, not merely the insert,
// so it fires identically whether the key would have been an insert or a
// replacement. There is no way to have a null key present any more, so this
// asserts the ordering directly: a populated dictionary is unchanged.
TEST(ListDictionaryNullKey, RejectionPrecedesTheLookupOnAPopulatedDictionary) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    d.Add(slot(2), slot(3));
    const auto before = versionOf(d);

    EXPECT_THROW(d.setItem(nullptr, slot(4)), System::ArgumentNullException);

    EXPECT_EQ(versionOf(d), before);
    EXPECT_EQ(d.getCountProperty(), 2);
    EXPECT_EQ(std::any_cast<void*>(d.getItem(slot(0))), slot(1));
    EXPECT_EQ(std::any_cast<void*>(d.getItem(slot(2))), slot(3));
}

TEST(ListDictionaryNullKey, ANullKeyCannotBeEnumeratedOrCopiedOutBecauseItCannotBeStored) {
    ListDictionaryInternal d;
    EXPECT_THROW(d.Add(nullptr, slot(1)), System::ArgumentNullException);
    EXPECT_THROW(d.setItem(nullptr, slot(1)), System::ArgumentNullException);

    Owning<IDictionaryEnumerator> walk(d.GetEnumerator());
    EXPECT_FALSE(walk->MoveNext());

    std::vector<DictionaryEntry> typed;
    EXPECT_NO_THROW(d.CopyTo(typed, 0));
    EXPECT_TRUE(typed.empty());
}

// The null key never aliased a real key here -- keys are compared by raw
// address and no valid object has the null address -- so rejecting it removes
// no legitimate key. That is measured, not assumed: it is the reason this
// ticket's null-key rationale is an INTERFACE-consistency one and NOT
// SR-AUD-363's aliasing one, which applied to Hashtable's stringified keys.
TEST(ListDictionaryNullKey, RejectingNullRemovesNoLegitimateKey) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    EXPECT_TRUE(d.Contains(slot(0)));
    EXPECT_THROW((void)d.Contains(nullptr), System::ArgumentNullException);
    EXPECT_EQ(d.getCountProperty(), 1) << "the real entry is untouched by the rejection";
}

// ===========================================================================
// 5. Exception ordering and rollback
// ===========================================================================

TEST(ListDictionaryRollback, ADuplicateAddCarriesTheExpectedExceptionAndChangesNothing) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    const auto before = versionOf(d);

    try {
        d.Add(slot(0), slot(2));
        ADD_FAILURE() << "a duplicate key must be rejected";
    } catch (const System::ArgumentException& e) {
        EXPECT_STREQ(e.what(), "Item has already been added.");
    }

    EXPECT_EQ(versionOf(d), before);
    EXPECT_EQ(d.getCountProperty(), 1);
    EXPECT_EQ(std::any_cast<void*>(d.getItem(slot(0))), slot(1));
}

// getItem on a missing key is not an error on this type: it yields an empty
// std::any, matching .NET's indexer getter returning null. Unchanged by #1798.
TEST(ListDictionaryRollback, AMissingGetItemYieldsAnEmptyAnyAndChangesNothing) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    const auto before = versionOf(d);

    EXPECT_FALSE(d.getItem(slot(9)).has_value());

    EXPECT_EQ(versionOf(d), before);
    EXPECT_EQ(d.getCountProperty(), 1);
}

// A replacement cannot throw: it assigns one pointer, which is noexcept. That
// is why the ordering validate -> locate -> mutate -> bump gives a NO-THROW
// guarantee on the replace branch and a STRONG one on the insert branch, where
// std::list::push_back is itself strongly exception-safe and the bump follows
// it. .NET's bump-first indexer setter can offer neither.
TEST(ListDictionaryRollback, TheReplaceBranchIsNoThrowAndTheInsertBranchIsStronglySafe) {
    static_assert(std::is_nothrow_assignable_v<void*&, void*>,
                  "the replace branch's only mutation is a noexcept pointer assignment");
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    EXPECT_NO_THROW(d.setItem(slot(0), slot(2)));

    // The insert branch: the bump is observably AFTER the push_back, so no path
    // exists on which the counter moved and the entry is absent.
    const auto before = versionOf(d);
    d.setItem(slot(5), slot(6));
    EXPECT_EQ(versionOf(d), before + 1);
    EXPECT_TRUE(d.Contains(slot(5)));
}

// ===========================================================================
// 6. Key and value representation, on every surface
// ===========================================================================

// The one rule, stated once and testable: a key is recovered with
// std::any_cast<const void*>, a value with std::any_cast<void*>, and an entry
// with std::any_cast<DictionaryEntry>, on EVERY surface. Before #1798 the key
// view's CopyTo was the single exception.
TEST(ListDictionaryRepresentation, EveryKeySurfaceBoxesConstVoidPointer) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));

    Owning<IDictionaryEnumerator> walk(d.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    const std::any enumeratorKey = walk->getKeyProperty();
    const std::any entryKey = walk->getEntryProperty().getKeyProperty();

    Owning<ICollection> keys(d.getKeysProperty());
    Owning<IEnumerator> keyWalk(keys->GetEnumerator());
    ASSERT_TRUE(keyWalk->MoveNext());
    const std::any viewCurrentKey = keyWalk->getCurrentProperty();

    std::vector<std::any> copiedKeys(1);
    keys->CopyTo(copiedKeys, 0);

    std::vector<DictionaryEntry> typed(1);
    d.CopyTo(typed, 0);
    const std::any typedKey = typed[0].getKeyProperty();

    EXPECT_EQ(enumeratorKey.type(), typeid(const void*));
    EXPECT_EQ(entryKey.type(), typeid(const void*));
    EXPECT_EQ(viewCurrentKey.type(), typeid(const void*));
    EXPECT_EQ(copiedKeys[0].type(), typeid(const void*)) << "the surface #1798 corrected";
    EXPECT_EQ(typedKey.type(), typeid(const void*));

    // All five name the same object.
    EXPECT_EQ(std::any_cast<const void*>(enumeratorKey), slot(0));
    EXPECT_EQ(std::any_cast<const void*>(entryKey), slot(0));
    EXPECT_EQ(std::any_cast<const void*>(viewCurrentKey), slot(0));
    EXPECT_EQ(std::any_cast<const void*>(copiedKeys[0]), slot(0));
    EXPECT_EQ(std::any_cast<const void*>(typedKey), slot(0));
}

TEST(ListDictionaryRepresentation, EveryValueSurfaceBoxesVoidPointer) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));

    Owning<IDictionaryEnumerator> walk(d.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    const std::any enumeratorValue = walk->getValueProperty();
    const std::any entryValue = walk->getEntryProperty().getValueProperty();

    Owning<ICollection> values(d.getValuesProperty());
    Owning<IEnumerator> valueWalk(values->GetEnumerator());
    ASSERT_TRUE(valueWalk->MoveNext());
    const std::any viewCurrentValue = valueWalk->getCurrentProperty();

    std::vector<std::any> copiedValues(1);
    values->CopyTo(copiedValues, 0);

    EXPECT_EQ(enumeratorValue.type(), typeid(void*));
    EXPECT_EQ(entryValue.type(), typeid(void*));
    EXPECT_EQ(viewCurrentValue.type(), typeid(void*));
    EXPECT_EQ(copiedValues[0].type(), typeid(void*));
    EXPECT_EQ(d.getItem(slot(0)).type(), typeid(void*));

    EXPECT_EQ(std::any_cast<void*>(enumeratorValue), slot(1));
    EXPECT_EQ(std::any_cast<void*>(copiedValues[0]), slot(1));
    EXPECT_EQ(std::any_cast<void*>(d.getItem(slot(0))), slot(1));
}

TEST(ListDictionaryRepresentation, TheEnumeratorsCurrentIsTheEntryNotTheKey) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));

    Owning<IDictionaryEnumerator> walk(d.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    const std::any current = walk->getCurrentProperty();

    ASSERT_EQ(current.type(), typeid(DictionaryEntry)) << "ticket #1794's contract, preserved";
    const auto entry = std::any_cast<DictionaryEntry>(current);
    EXPECT_EQ(std::any_cast<const void*>(entry.getKeyProperty()), slot(0));
    EXPECT_EQ(std::any_cast<void*>(entry.getValueProperty()), slot(1));
}

// No accidental nesting: the box holds the pointer, never a std::any wrapping
// another std::any.
TEST(ListDictionaryRepresentation, NoSurfaceProducesANestedAny) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));

    Owning<ICollection> keys(d.getKeysProperty());
    std::vector<std::any> copiedKeys(1);
    keys->CopyTo(copiedKeys, 0);

    EXPECT_NE(copiedKeys[0].type(), typeid(std::any));
    EXPECT_NE(d.getItem(slot(0)).type(), typeid(std::any));
}

// The old spelling still COMPILES -- std::any_cast<void*> is well-formed on any
// std::any -- and now throws at run time. That is the single silent
// source-compatible meaning change in ticket #1798, and it is pinned here so
// the migration note in README.md has a test behind it.
TEST(ListDictionaryRepresentation, TheSupersededKeyCastCompilesAndThrowsAtRunTime) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    Owning<ICollection> keys(d.getKeysProperty());
    std::vector<std::any> copiedKeys(1);
    keys->CopyTo(copiedKeys, 0);

    EXPECT_THROW((void)std::any_cast<void*>(copiedKeys[0]), std::bad_any_cast);
    EXPECT_NO_THROW((void)std::any_cast<const void*>(copiedKeys[0]));
}

// The key view's CopyTo used to manufacture a writable pointer to an object the
// CALLER declared const, and writing through it was an AddressSanitizer SEGV on
// read-only storage. No writable pointer is published any more -- from either
// key surface -- so the write is unreachable rather than merely discouraged.
const int kReadOnlyKey = 7;

TEST(ListDictionaryRepresentation, AKeyInReadOnlyStorageIsNeverPublishedAsWritable) {
    ListDictionaryInternal d;
    d.Add(&kReadOnlyKey, slot(1));

    Owning<ICollection> keys(d.getKeysProperty());
    std::vector<std::any> copiedKeys(1);
    keys->CopyTo(copiedKeys, 0);
    EXPECT_THROW((void)std::any_cast<void*>(copiedKeys[0]), std::bad_any_cast);
    EXPECT_EQ(std::any_cast<const void*>(copiedKeys[0]), &kReadOnlyKey);

    Owning<IEnumerator> keyWalk(keys->GetEnumerator());
    ASSERT_TRUE(keyWalk->MoveNext());
    EXPECT_THROW((void)std::any_cast<void*>(keyWalk->getCurrentProperty()), std::bad_any_cast);

    Owning<IDictionaryEnumerator> walk(d.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    EXPECT_THROW((void)std::any_cast<void*>(walk->getKeyProperty()), std::bad_any_cast);

    EXPECT_EQ(kReadOnlyKey, 7) << "and the caller's const object is intact";
}

TEST(ListDictionaryRepresentation, CopyToOrderingAndCapacityValidationAreUnchanged) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    d.Add(slot(2), slot(3));
    Owning<ICollection> keys(d.getKeysProperty());

    std::vector<std::any> exact(2);
    ASSERT_NO_THROW(keys->CopyTo(exact, 0));
    EXPECT_EQ(std::any_cast<const void*>(exact[0]), slot(0)) << "insertion order preserved";
    EXPECT_EQ(std::any_cast<const void*>(exact[1]), slot(2));

    std::vector<std::any> tooSmall(1);
    EXPECT_THROW(keys->CopyTo(tooSmall, 0), System::ArgumentException);
    std::vector<std::any> offset(4);
    EXPECT_THROW(keys->CopyTo(offset, -1), System::ArgumentOutOfRangeException);
}

TEST(ListDictionaryRepresentation, CopyToDoesNotChangeCountOrTheVersion) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    const auto before = versionOf(d);

    Owning<ICollection> keys(d.getKeysProperty());
    std::vector<std::any> copied(1);
    keys->CopyTo(copied, 0);
    std::vector<DictionaryEntry> typed(1);
    d.CopyTo(typed, 0);

    EXPECT_EQ(versionOf(d), before);
    EXPECT_EQ(d.getCountProperty(), 1);
}

// ===========================================================================
// 7. Value payload shapes
// ===========================================================================

/** @brief Counts its own construction and destruction, for ownership balance. */
struct Counted {
    static int alive;
    int payload;
    explicit Counted(int p) : payload(p) { ++alive; }
    Counted(const Counted& other) : payload(other.payload) { ++alive; }
    Counted& operator=(const Counted&) = default;
    ~Counted() { --alive; }
};
int Counted::alive = 0;

TEST(ListDictionaryValues, StoresAndReturnsEveryPointerShapeUnchanged) {
    ListDictionaryInternal d;

    int asInt = 5;
    std::string asString = "payload";
    std::any asAny = std::any(std::string("boxed"));
    int* asRawPointer = slot(1);
    auto asShared = std::make_shared<std::string>("shared");
    Counted asCounted(11);

    int k0 = 0, k1 = 0, k2 = 0, k3 = 0, k4 = 0, k5 = 0;
    d.Add(&k0, &asInt);
    d.Add(&k1, &asString);
    d.Add(&k2, &asAny);
    d.Add(&k3, &asRawPointer);
    d.Add(&k4, asShared.get());
    d.Add(&k5, &asCounted);

    EXPECT_EQ(std::any_cast<void*>(d.getItem(&k0)), &asInt);
    EXPECT_EQ(std::any_cast<void*>(d.getItem(&k1)), &asString);
    // A std::any-valued object round-trips as the CALLER's address: this
    // dictionary never copies a pointee, so nesting is the caller's business.
    EXPECT_EQ(std::any_cast<void*>(d.getItem(&k2)), &asAny);
    EXPECT_EQ(std::any_cast<void*>(d.getItem(&k3)), &asRawPointer);
    EXPECT_EQ(std::any_cast<void*>(d.getItem(&k4)), asShared.get());
    EXPECT_EQ(std::any_cast<void*>(d.getItem(&k5)), &asCounted);
    EXPECT_EQ(d.getCountProperty(), 6);
}

// The dictionary owns nothing, so no replacement, Clear or destruction may
// construct or destroy a value. The counter proves it rather than assuming it.
TEST(ListDictionaryValues, NoOwnershipIsTakenOfAnyValue) {
    const int aliveBefore = Counted::alive;
    Counted first(1);
    Counted second(2);
    {
        ListDictionaryInternal d;
        int key = 0;
        d.Add(&key, &first);
        EXPECT_EQ(Counted::alive, aliveBefore + 2);
        d.setItem(&key, &second);
        EXPECT_EQ(Counted::alive, aliveBefore + 2) << "a replacement copies nothing";
        d.Clear();
        EXPECT_EQ(Counted::alive, aliveBefore + 2) << "Clear destroys nothing it does not own";
    }
    EXPECT_EQ(Counted::alive, aliveBefore + 2)
        << "and the dictionary's own destruction destroys nothing either";
    EXPECT_EQ(first.payload, 1);
    EXPECT_EQ(second.payload, 2);
}

// Mutating the object a value POINTS AT is not a dictionary mutation and must
// not bump; replacing the stored pointer is one and must. Two levels, matching
// .NET's reference semantics.
TEST(ListDictionaryValues, MutatingAPointeeIsNotADictionaryMutation) {
    ListDictionaryInternal d;
    int key = 0;
    int value = 1;
    d.Add(&key, &value);
    const auto before = versionOf(d);

    value = 8;
    EXPECT_EQ(versionOf(d), before) << "the dictionary cannot see through the pointer";

    int other = 9;
    d.setItem(&key, &other);
    EXPECT_EQ(versionOf(d), before + 1) << "but rebinding the slot is a mutation";
}

TEST(ListDictionaryValues, ANullValueUnderAValidKeyStaysDistinguishableFromAbsent) {
    ListDictionaryInternal d;
    int key = 0;
    d.Add(&key, nullptr);

    ASSERT_TRUE(d.getItem(&key).has_value()) << "present with a null value";
    EXPECT_EQ(std::any_cast<void*>(d.getItem(&key)), nullptr);
    EXPECT_FALSE(d.getItem(slot(9)).has_value()) << "absent";
    EXPECT_TRUE(d.Contains(&key));
}

// ===========================================================================
// 8. Key semantics
// ===========================================================================

TEST(ListDictionaryKeys, DistinctAddressesWithEqualContentsAreDistinctKeys) {
    ListDictionaryInternal d;
    int first = 42;
    int second = 42;   // equal contents, different address
    ASSERT_NE(&first, &second);

    d.Add(&first, slot(1));
    EXPECT_TRUE(d.Contains(&first));
    EXPECT_FALSE(d.Contains(&second))
        << "keys are compared by pointer identity -- a documented permanent limitation";

    EXPECT_NO_THROW(d.Add(&second, slot(2))) << "so this is not a duplicate";
    EXPECT_EQ(d.getCountProperty(), 2);
}

TEST(ListDictionaryKeys, PointerIdentityIsPreservedThroughEverySurface) {
    ListDictionaryInternal d;
    int key = 0;
    d.Add(&key, slot(1));

    Owning<IDictionaryEnumerator> walk(d.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    EXPECT_EQ(std::any_cast<const void*>(walk->getKeyProperty()),
              static_cast<const void*>(&key));

    Owning<ICollection> keys(d.getKeysProperty());
    std::vector<std::any> copied(1);
    keys->CopyTo(copied, 0);
    EXPECT_EQ(std::any_cast<const void*>(copied[0]), static_cast<const void*>(&key));
}

TEST(ListDictionaryKeys, AConstStorageKeyIsAcceptedAndReturnedAsConst) {
    ListDictionaryInternal d;
    d.setItem(&kReadOnlyKey, slot(1));
    EXPECT_TRUE(d.Contains(&kReadOnlyKey));
    EXPECT_EQ(std::any_cast<void*>(d.getItem(&kReadOnlyKey)), slot(1));
    d.Remove(&kReadOnlyKey);
    EXPECT_EQ(d.getCountProperty(), 0);
}

// ===========================================================================
// 9. Copy, move and assignment -- ticket #1787's contract, unchanged by #1798
// ===========================================================================

TEST(ListDictionaryAssignment, CopyAndMoveConstructionCarryTheContents) {
    ListDictionaryInternal source;
    source.Add(slot(0), slot(1));

    ListDictionaryInternal copied = source;
    EXPECT_EQ(copied.getCountProperty(), 1);
    EXPECT_EQ(std::any_cast<void*>(copied.getItem(slot(0))), slot(1));

    ListDictionaryInternal moved = std::move(source);
    EXPECT_EQ(moved.getCountProperty(), 1);
}

// The destination's counter ADVANCES rather than taking the source's value
// (ticket #1787), so an enumerator outstanding over the destination is
// invalidated by an assignment that destroyed everything it could refer to.
TEST(ListDictionaryAssignment, AssignmentAdvancesTheDestinationsOwnCounter) {
    ListDictionaryInternal destination;
    destination.Add(slot(0), slot(1));
    ListDictionaryInternal source;
    source.Add(slot(2), slot(3));

    const auto before = versionOf(destination);
    Owning<IDictionaryEnumerator> walk(destination.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());

    destination = source;

    EXPECT_EQ(versionOf(destination), before + 1);
    EXPECT_THROW((void)walk->MoveNext(), System::InvalidOperationException);
}

TEST(ListDictionaryAssignment, MoveAssignmentInvalidatesTheDestinationToo) {
    ListDictionaryInternal destination;
    destination.Add(slot(0), slot(1));
    ListDictionaryInternal source;
    source.Add(slot(2), slot(3));

    Owning<IDictionaryEnumerator> walk(destination.GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());

    destination = std::move(source);

    EXPECT_THROW((void)walk->MoveNext(), System::InvalidOperationException);
}

// Self-assignment also advances, deliberately and conservatively: it is the
// memory-safe answer, and #1798 does not change it.
TEST(ListDictionaryAssignment, SelfAssignmentAdvancesTheCounter) {
    ListDictionaryInternal d;
    d.Add(slot(0), slot(1));
    const auto before = versionOf(d);

    ListDictionaryInternal& alias = d;
    d = alias;

    EXPECT_EQ(versionOf(d), before + 1);
    EXPECT_EQ(d.getCountProperty(), 1) << "and the contents survive";
    EXPECT_EQ(std::any_cast<void*>(d.getItem(slot(0))), slot(1));
}

// A validated key must keep working across an assignment: the ValidatedKey
// boundary is per-call and holds no state.
TEST(ListDictionaryAssignment, TheNullKeyContractSurvivesAssignment) {
    ListDictionaryInternal destination;
    ListDictionaryInternal source;
    source.Add(slot(0), slot(1));
    destination = source;

    EXPECT_THROW(destination.Add(nullptr, slot(2)), System::ArgumentNullException);
    EXPECT_THROW((void)destination.Contains(nullptr), System::ArgumentNullException);
    EXPECT_EQ(destination.getCountProperty(), 1);
}

// ===========================================================================
// 10. Layout -- #1798 changes no data member, so nothing here may move
// ===========================================================================

TEST(ListDictionaryLayout, TheObjectSizeIsUnchangedByTicket1798) {
    EXPECT_EQ(sizeof(ListDictionaryInternal), 40u)
        << "ValidatedKey is a private per-call temporary, never a data member";
    EXPECT_EQ(alignof(ListDictionaryInternal), 8u);
}

} // namespace
