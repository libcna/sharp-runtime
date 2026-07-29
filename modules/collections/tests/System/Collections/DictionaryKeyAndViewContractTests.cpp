// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for the non-generic IDictionary key and view
// contracts (ticket #1775, audit finding SR-AUD-363).
//
// Two defects are covered:
//
//   1. Hashtable::getKeysProperty()/getValuesProperty() returned nullptr while
//      IDictionary documents each as returning an ICollection over the
//      dictionary's keys/values, so a consumer written against the interface
//      dereferenced null. Because that is an *interface* defect rather than a
//      Hashtable typo, the view cases are parameterised over every non-generic
//      IDictionary implementation in the repository -- Hashtable and
//      ListDictionaryInternal -- so neither implementation can regress to a
//      null or snapshot view.
//
//   2. Hashtable's raw-key entry points stringified a null key as the address
//      text "0", so Add/setItem/getItem/Contains/Remove all accepted a null key
//      instead of throwing ArgumentNullException, and that stringified null key
//      aliased the ordinary string key "0" accepted by the
//      Add(const std::string&, const std::any&) overload. The C-string Remove
//      overload additionally terminated on std::string's null construction.
//
// The views deliberately reuse the ticket #1771/#1774 copy boundary, so the
// destination cases here assert that ICollection::CopyTo still owns validation
// for a view exactly as it does for a concrete collection.
#include <gtest/gtest.h>

#include <any>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"

using SharpRuntime::intcs;
using System::Collections::Hashtable;
using System::Collections::ICollection;
using System::Collections::IDictionary;
using System::Collections::IDictionaryEnumerator;
using System::Collections::IEnumerator;
using System::Collections::ListDictionaryInternal;
using System::Collections::ObjectSpan;

namespace {

// ---------------------------------------------------------------------------
// Harness: one uniform way to populate either non-generic IDictionary, whose
// key spaces differ (Hashtable keys are strings, ListDictionaryInternal keys
// are compared by pointer identity).
// ---------------------------------------------------------------------------

class DictionaryHarness {
public:
    virtual ~DictionaryHarness() = default;
    virtual IDictionary& dictionary() = 0;
    /** @brief Adds @p count entries that are not already present. */
    virtual void addEntries(int count) = 0;
    virtual const char* name() const = 0;
};

class HashtableHarness : public DictionaryHarness {
    Hashtable table_;
    int next_ = 0;

public:
    IDictionary& dictionary() override { return table_; }
    void addEntries(int count) override {
        for (int i = 0; i < count; ++i, ++next_)
            table_.Add(std::string("key") + std::to_string(next_), std::any(next_));
    }
    const char* name() const override { return "Hashtable"; }
};

class ListDictionaryInternalHarness : public DictionaryHarness {
    ListDictionaryInternal dictionary_;
    // Stable storage: the dictionary keys and values are borrowed pointers.
    std::deque<int> keys_;
    std::deque<int> values_;

public:
    IDictionary& dictionary() override { return dictionary_; }
    void addEntries(int count) override {
        for (int i = 0; i < count; ++i) {
            keys_.push_back(static_cast<int>(keys_.size()));
            values_.push_back(static_cast<int>(values_.size()) * 10);
            dictionary_.Add(static_cast<const void*>(&keys_.back()), &values_.back());
        }
    }
    const char* name() const override { return "ListDictionaryInternal"; }
};

enum class Implementation { Hashtable, ListDictionaryInternal };

std::unique_ptr<DictionaryHarness> makeHarness(Implementation which) {
    if (which == Implementation::Hashtable)
        return std::make_unique<HashtableHarness>();
    return std::make_unique<ListDictionaryInternalHarness>();
}

// ---------------------------------------------------------------------------
// Shared IDictionary view contract, over every non-generic implementation
// ---------------------------------------------------------------------------

class DictionaryViewContract : public ::testing::TestWithParam<Implementation> {
protected:
    void SetUp() override { harness_ = makeHarness(GetParam()); }

    IDictionary& dictionary() { return harness_->dictionary(); }
    void addEntries(int count) { harness_->addEntries(count); }

    std::unique_ptr<ICollection> keysView() {
        return std::unique_ptr<ICollection>(dictionary().getKeysProperty());
    }
    std::unique_ptr<ICollection> valuesView() {
        return std::unique_ptr<ICollection>(dictionary().getValuesProperty());
    }

private:
    std::unique_ptr<DictionaryHarness> harness_;
};

INSTANTIATE_TEST_SUITE_P(
    AllNonGenericDictionaries, DictionaryViewContract,
    ::testing::Values(Implementation::Hashtable, Implementation::ListDictionaryInternal),
    [](const ::testing::TestParamInfo<Implementation>& info) {
        return info.param == Implementation::Hashtable ? "Hashtable" : "ListDictionaryInternal";
    });

TEST_P(DictionaryViewContract, EmptyDictionaryStillReturnsNonNullViews) {
    auto keys = keysView();
    auto values = valuesView();
    ASSERT_NE(keys, nullptr);
    ASSERT_NE(values, nullptr);
    EXPECT_EQ(keys->getCountProperty(), 0);
    EXPECT_EQ(values->getCountProperty(), 0);
}

TEST_P(DictionaryViewContract, SingleEntryDictionaryReturnsNonNullViews) {
    addEntries(1);
    auto keys = keysView();
    auto values = valuesView();
    ASSERT_NE(keys, nullptr);
    ASSERT_NE(values, nullptr);
    EXPECT_EQ(keys->getCountProperty(), 1);
    EXPECT_EQ(values->getCountProperty(), 1);
}

TEST_P(DictionaryViewContract, MultipleEntryDictionaryReturnsNonNullViews) {
    addEntries(5);
    auto keys = keysView();
    auto values = valuesView();
    ASSERT_NE(keys, nullptr);
    ASSERT_NE(values, nullptr);
    EXPECT_EQ(keys->getCountProperty(), 5);
    EXPECT_EQ(values->getCountProperty(), 5);
}

// The exact pre-fix crash: a consumer that uses the documented view without a
// null check. Before ticket #1775 this was an ASan-confirmed SEGV against
// Hashtable and worked against ListDictionaryInternal.
TEST_P(DictionaryViewContract, ContractFollowingConsumerNeedsNoNullCheck) {
    addEntries(3);
    const IDictionary& asInterface = dictionary();
    std::unique_ptr<ICollection> keys(asInterface.getKeysProperty());
    EXPECT_EQ(keys->getCountProperty(), 3);
}

TEST_P(DictionaryViewContract, ViewReflectsLaterAdditions) {
    auto keys = keysView();
    auto values = valuesView();
    EXPECT_EQ(keys->getCountProperty(), 0);
    addEntries(2);
    EXPECT_EQ(keys->getCountProperty(), 2) << "the key view must be live, not a snapshot";
    EXPECT_EQ(values->getCountProperty(), 2) << "the value view must be live, not a snapshot";
}

TEST_P(DictionaryViewContract, ViewReflectsLaterClear) {
    addEntries(4);
    auto keys = keysView();
    ASSERT_EQ(keys->getCountProperty(), 4);
    dictionary().Clear();
    EXPECT_EQ(keys->getCountProperty(), 0);
}

TEST_P(DictionaryViewContract, EachCallReturnsAnIndependentlyOwnedView) {
    addEntries(2);
    std::unique_ptr<ICollection> first(dictionary().getKeysProperty());
    std::unique_ptr<ICollection> second(dictionary().getKeysProperty());
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first.get(), second.get())
        << "each call hands back a separately owned object; deleting one must not "
           "invalidate the other";
    EXPECT_EQ(first->getCountProperty(), second->getCountProperty());
    first.reset();
    EXPECT_EQ(second->getCountProperty(), 2) << "deleting one view must not disturb another";
}

TEST_P(DictionaryViewContract, ViewSynchronizationMirrorsTheOwningDictionary) {
    addEntries(1);
    auto keys = keysView();
    auto values = valuesView();
    EXPECT_EQ(keys->getSyncRootProperty(), dictionary().getSyncRootProperty());
    EXPECT_EQ(values->getSyncRootProperty(), dictionary().getSyncRootProperty());
    EXPECT_EQ(keys->getIsSynchronizedProperty(), dictionary().getIsSynchronizedProperty());
    EXPECT_EQ(values->getIsSynchronizedProperty(), dictionary().getIsSynchronizedProperty());
}

TEST_P(DictionaryViewContract, ViewEnumeratorIsNeverNullAndVisitsEveryElement) {
    addEntries(3);
    auto keys = keysView();
    std::unique_ptr<IEnumerator> walk(keys->GetEnumerator());
    ASSERT_NE(walk, nullptr);
    int visited = 0;
    while (walk->MoveNext()) {
        EXPECT_TRUE(walk->getCurrentProperty().has_value());
        ++visited;
    }
    EXPECT_EQ(visited, 3);
}

TEST_P(DictionaryViewContract, ViewEnumeratorOverAnEmptyDictionaryVisitsNothing) {
    auto keys = keysView();
    std::unique_ptr<IEnumerator> walk(keys->GetEnumerator());
    ASSERT_NE(walk, nullptr);
    EXPECT_FALSE(walk->MoveNext());
}

TEST_P(DictionaryViewContract, ViewEnumeratorCurrentBeforeMoveNextThrows) {
    addEntries(1);
    auto keys = keysView();
    std::unique_ptr<IEnumerator> walk(keys->GetEnumerator());
    EXPECT_THROW((void)walk->getCurrentProperty(), System::InvalidOperationException);
}

TEST_P(DictionaryViewContract, ViewEnumeratorCurrentAfterLastElementThrows) {
    addEntries(1);
    auto keys = keysView();
    std::unique_ptr<IEnumerator> walk(keys->GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    ASSERT_FALSE(walk->MoveNext());
    EXPECT_THROW((void)walk->getCurrentProperty(), System::InvalidOperationException);
}

TEST_P(DictionaryViewContract, ViewEnumeratorResetRestartsEnumeration) {
    addEntries(2);
    auto keys = keysView();
    std::unique_ptr<IEnumerator> walk(keys->GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());
    ASSERT_TRUE(walk->MoveNext());
    ASSERT_FALSE(walk->MoveNext());
    walk->Reset();
    int visited = 0;
    while (walk->MoveNext()) ++visited;
    EXPECT_EQ(visited, 2);
}

// ---------------------------------------------------------------------------
// The views go through the ticket #1771/#1774 copy boundary unchanged
// ---------------------------------------------------------------------------

TEST_P(DictionaryViewContract, ViewCopyToFillsAnExactFitDestination) {
    addEntries(3);
    auto keys = keysView();
    std::vector<std::any> destination(3);
    keys->CopyTo(destination, 0);
    for (const auto& slot : destination)
        EXPECT_TRUE(slot.has_value()) << "every destination slot must be written";
}

TEST_P(DictionaryViewContract, ViewCopyToAtAnOffsetLeavesSurroundingSlotsUntouched) {
    addEntries(2);
    auto keys = keysView();
    std::vector<std::any> destination(4);
    destination[0] = std::any(std::string("untouched-front"));
    destination[3] = std::any(std::string("untouched-back"));
    keys->CopyTo(destination, 1);
    EXPECT_EQ(std::any_cast<std::string>(destination[0]), "untouched-front");
    EXPECT_EQ(std::any_cast<std::string>(destination[3]), "untouched-back");
    EXPECT_TRUE(destination[1].has_value());
    EXPECT_TRUE(destination[2].has_value());
}

TEST_P(DictionaryViewContract, ViewCopyToRejectsAnUndersizedDestination) {
    addEntries(3);
    auto keys = keysView();
    std::vector<std::any> destination(2);
    EXPECT_THROW(keys->CopyTo(destination, 0), System::ArgumentException);
}

TEST_P(DictionaryViewContract, ViewCopyToRejectsANegativeIndex) {
    addEntries(1);
    auto keys = keysView();
    std::vector<std::any> destination(4);
    EXPECT_THROW(keys->CopyTo(destination, -1), System::ArgumentOutOfRangeException);
}

TEST_P(DictionaryViewContract, ViewCopyToRejectsAMalformedNullDestinationWithPositiveLength) {
    addEntries(1);
    auto keys = keysView();
    EXPECT_THROW(keys->CopyTo(ObjectSpan(nullptr, 4), 0), System::ArgumentNullException);
}

TEST_P(DictionaryViewContract, EmptyViewIntoAnEmptyDestinationSucceeds) {
    auto keys = keysView();
    std::vector<std::any> destination;
    EXPECT_NO_THROW(keys->CopyTo(destination, 0));
    EXPECT_NO_THROW(keys->CopyTo(ObjectSpan(nullptr, 0), 0));
}

TEST_P(DictionaryViewContract, NonEmptyViewIntoAZeroLengthDestinationFailsOnCapacity) {
    addEntries(1);
    auto keys = keysView();
    std::vector<std::any> destination;
    EXPECT_THROW(keys->CopyTo(destination, 0), System::ArgumentException);
}

TEST_P(DictionaryViewContract, ViewCopyToAtTheMaximumIndexDoesNotOverflow) {
    addEntries(1);
    auto keys = keysView();
    std::vector<std::any> destination(4);
    EXPECT_THROW(keys->CopyTo(destination, std::numeric_limits<intcs>::max()),
                 System::ArgumentException);
}

TEST_P(DictionaryViewContract, ViewCopyToWritesNothingWhenValidationFails) {
    addEntries(2);
    auto keys = keysView();
    std::vector<std::any> destination(1);
    destination[0] = std::any(std::string("sentinel"));
    EXPECT_THROW(keys->CopyTo(destination, 0), System::ArgumentException);
    EXPECT_EQ(std::any_cast<std::string>(destination[0]), "sentinel");
}

// A view of one dictionary must never be satisfied by another dictionary's
// capacity: the boundary validates against the view's own count.
TEST_P(DictionaryViewContract, ViewValidatesAgainstItsOwnCountNotAnotherDictionarys) {
    addEntries(4);
    auto keys = keysView();

    auto otherHarness = makeHarness(GetParam());
    otherHarness->addEntries(1);
    std::unique_ptr<ICollection> otherKeys(otherHarness->dictionary().getKeysProperty());

    std::vector<std::any> destinationForOne(1);
    EXPECT_NO_THROW(otherKeys->CopyTo(destinationForOne, 0));
    EXPECT_THROW(keys->CopyTo(destinationForOne, 0), System::ArgumentException);
}

TEST_P(DictionaryViewContract, ViewDeletedBeforeItsOwnerIsClean) {
    addEntries(2);
    ICollection* keys = dictionary().getKeysProperty();
    ASSERT_NE(keys, nullptr);
    EXPECT_EQ(keys->getCountProperty(), 2);
    delete keys;
    // The dictionary is unaffected by the view's destruction.
    EXPECT_EQ(dictionary().getCountProperty(), 2);
}

// ---------------------------------------------------------------------------
// Non-generic IDictionary null-key contract, over EVERY implementation
//
// Ticket #1775 established rejection on Hashtable; ticket #1798 established it
// on ListDictionaryInternal, which until then accepted and STORED a null key on
// all five raw-key entry points. Because that made the two implementations of
// one interface disagree -- so no polymorphic IDictionary consumer could rely on
// the contract at all -- these five cases are GENERALISED over both
// implementations rather than duplicated per type: every one of them is spelled
// through the IDictionary& base reference, which is the surface the contract
// belongs to. The Hashtable-only cases that follow cover the overloads and the
// stringified-key aliasing that exist on that implementation alone.
// ---------------------------------------------------------------------------

// Ticket #1776 (REMED-CORE-ARGNULL-MESSAGE) corrected
// System::ArgumentNullException(paramName) so it emits its "(Parameter 'key')"
// suffix exactly once instead of twice; this suite previously asserted only
// the exception type, the named parameter, and the leading null message text
// to stay correct across that pre-existing Core.Base defect (which also
// affected the CopyTo boundary's ArgumentNullException("destination")), and
// now asserts the exact, single-suffix message that ticket #1776 restored.
void expectNullKeyRejected(const std::function<void()>& call) {
    try {
        call();
        ADD_FAILURE() << "a null key must be rejected";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "key");
        EXPECT_STREQ(e.what(), "Value cannot be null. (Parameter 'key')");
    } catch (const std::exception& e) {
        ADD_FAILURE() << "expected System::ArgumentNullException, got: " << e.what();
    }
}

class DictionaryNullKey : public ::testing::TestWithParam<Implementation> {
protected:
    void SetUp() override { harness_ = makeHarness(GetParam()); }

    IDictionary& dictionary() { return harness_->dictionary(); }
    void addEntries(int count) { harness_->addEntries(count); }

private:
    std::unique_ptr<DictionaryHarness> harness_;
};

INSTANTIATE_TEST_SUITE_P(
    AllNonGenericDictionaries, DictionaryNullKey,
    ::testing::Values(Implementation::Hashtable, Implementation::ListDictionaryInternal),
    [](const ::testing::TestParamInfo<Implementation>& info) {
        return info.param == Implementation::Hashtable ? "Hashtable" : "ListDictionaryInternal";
    });

TEST_P(DictionaryNullKey, AddRejectsANullRawKey) {
    std::any value = std::any(1);
    expectNullKeyRejected([&] { dictionary().Add(static_cast<const void*>(nullptr), &value); });
}

TEST_P(DictionaryNullKey, SetItemRejectsANullRawKey) {
    std::any value = std::any(1);
    expectNullKeyRejected([&] { dictionary().setItem(nullptr, &value); });
}

TEST_P(DictionaryNullKey, GetItemRejectsANullRawKey) {
    expectNullKeyRejected([&] { (void)dictionary().getItem(nullptr); });
}

TEST_P(DictionaryNullKey, ContainsRejectsANullRawKey) {
    expectNullKeyRejected([&] { (void)dictionary().Contains(nullptr); });
}

TEST_P(DictionaryNullKey, RemoveRejectsANullRawKey) {
    expectNullKeyRejected([&] { dictionary().Remove(static_cast<const void*>(nullptr)); });
}

TEST_P(DictionaryNullKey, ARejectedNullKeyLeavesTheDictionaryUnmodified) {
    addEntries(1);
    std::any value = std::any(2);

    EXPECT_THROW(dictionary().Add(static_cast<const void*>(nullptr), &value),
                 System::ArgumentNullException);
    EXPECT_THROW(dictionary().setItem(nullptr, &value), System::ArgumentNullException);
    EXPECT_THROW(dictionary().Remove(static_cast<const void*>(nullptr)),
                 System::ArgumentNullException);
    EXPECT_THROW((void)dictionary().getItem(nullptr), System::ArgumentNullException);
    EXPECT_THROW((void)dictionary().Contains(nullptr), System::ArgumentNullException);

    EXPECT_EQ(dictionary().getCountProperty(), 1)
        << "no rejected call may insert, replace or erase anything";
}

// A rejected call must not bump the fail-fast version counter, or an in-flight
// enumeration would be invalidated by an operation that changed nothing.
TEST_P(DictionaryNullKey, ARejectedNullKeyDoesNotInvalidateAnInFlightEnumeration) {
    addEntries(2);

    std::unique_ptr<IDictionaryEnumerator> walk(dictionary().GetEnumerator());
    ASSERT_TRUE(walk->MoveNext());

    std::any value = std::any(3);
    EXPECT_THROW(dictionary().Add(static_cast<const void*>(nullptr), &value),
                 System::ArgumentNullException);
    EXPECT_THROW(dictionary().setItem(nullptr, &value), System::ArgumentNullException);
    EXPECT_THROW(dictionary().Remove(static_cast<const void*>(nullptr)),
                 System::ArgumentNullException);

    EXPECT_NO_THROW((void)walk->MoveNext())
        << "a rejected argument must not count as a structural modification";
}

// A null key must not be storable, findable, enumerable or copyable out. Before
// ticket #1798, ListDictionaryInternal stored one and every one of those
// succeeded; Hashtable had the same shape before #1775, where the stringified
// null additionally aliased the ordinary string key "0".
TEST_P(DictionaryNullKey, ANullKeyNeverBecomesAnEntry) {
    std::any value = std::any(1);
    EXPECT_THROW(dictionary().Add(static_cast<const void*>(nullptr), &value),
                 System::ArgumentNullException);
    EXPECT_THROW(dictionary().setItem(nullptr, &value), System::ArgumentNullException);

    EXPECT_EQ(dictionary().getCountProperty(), 0);

    std::unique_ptr<IDictionaryEnumerator> walk(dictionary().GetEnumerator());
    EXPECT_FALSE(walk->MoveNext()) << "nothing was stored, so nothing can be enumerated";

    std::vector<std::any> destination;
    EXPECT_NO_THROW(dictionary().CopyTo(destination, 0))
        << "an empty dictionary copies into an empty destination (ticket #1774)";
}

// Before ticket #1775 this reached std::string's null construction and
// terminated with a std::logic_error that code catching System::Exception&
// cannot see. The overload is Hashtable's alone, so this case is not
// parameterised.
TEST(HashtableNullKey, RemoveRejectsANullCStringKey) {
    Hashtable table;
    expectNullKeyRejected([&] { table.Remove(static_cast<const char*>(nullptr)); });
}

// The stringified null key was the address text "0", which collides with the
// ordinary string key "0" the std::string overload accepts.
TEST(HashtableNullKey, ANullKeyNoLongerAliasesTheStringKeyZero) {
    Hashtable table;
    table.Add(std::string("0"), std::any(7));
    ASSERT_TRUE(table.ContainsKey("0"));
    ASSERT_EQ(table.getCountProperty(), 1);

    EXPECT_THROW((void)table.Contains(nullptr), System::ArgumentNullException);
    EXPECT_THROW(table.Remove(static_cast<const void*>(nullptr)), System::ArgumentNullException);

    EXPECT_EQ(table.getCountProperty(), 1) << "the entry stored under \"0\" is untouched";
    EXPECT_EQ(std::any_cast<int>(table.at("0")), 7);
}

TEST(HashtableNullKey, AddingTheStringKeyZeroIsNotADuplicateOfSomeNullKey) {
    Hashtable table;
    EXPECT_NO_THROW(table.Add(std::string("0"), std::any(1)));
    EXPECT_EQ(table.getCountProperty(), 1);
}

TEST(HashtableNullKey, NonNullRawKeysStillWork) {
    Hashtable table;
    int firstAnchor = 1;
    int secondAnchor = 2;
    std::any firstValue = std::any(10);
    std::any secondValue = std::any(20);

    table.Add(static_cast<const void*>(&firstAnchor), &firstValue);
    table.Add(static_cast<const void*>(&secondAnchor), &secondValue);

    EXPECT_EQ(table.getCountProperty(), 2);
    EXPECT_TRUE(table.Contains(&firstAnchor));
    EXPECT_TRUE(table.Contains(&secondAnchor));
    // Ticket #1796: getItem() returns an owning std::any by value. A present key is
    // now tested with has_value() rather than against nullptr, and the value is
    // recovered with one std::any_cast instead of a static_cast through a raw pointer
    // into live map storage.
    ASSERT_TRUE(table.getItem(&firstAnchor).has_value());
    EXPECT_EQ(std::any_cast<int>(table.getItem(&firstAnchor)), 10);

    table.Remove(static_cast<const void*>(&firstAnchor));
    EXPECT_FALSE(table.Contains(&firstAnchor));
    EXPECT_EQ(table.getCountProperty(), 1);
}

// ---------------------------------------------------------------------------
// Hashtable view element shapes, copy/move, and lifetime
// ---------------------------------------------------------------------------

TEST(HashtableViews, KeyViewBoxesTheKeyAsAString) {
    Hashtable table;
    table.Add(std::string("only-key"), std::any(1));
    std::unique_ptr<ICollection> keys(table.getKeysProperty());

    std::vector<std::any> destination(1);
    keys->CopyTo(destination, 0);
    EXPECT_EQ(std::any_cast<std::string>(destination[0]), "only-key");
}

TEST(HashtableViews, ValueViewCopiesTheStoredValueItself) {
    Hashtable table;
    table.Add(std::string("k"), std::any(std::string("stored-value")));
    std::unique_ptr<ICollection> values(table.getValuesProperty());

    std::vector<std::any> destination(1);
    values->CopyTo(destination, 0);
    EXPECT_EQ(std::any_cast<std::string>(destination[0]), "stored-value");
}

TEST(HashtableViews, KeyAndValueViewsAgreeOnOrder) {
    Hashtable table;
    table.Add(std::string("a"), std::any(std::string("value-a")));
    table.Add(std::string("b"), std::any(std::string("value-b")));
    table.Add(std::string("c"), std::any(std::string("value-c")));

    std::unique_ptr<ICollection> keys(table.getKeysProperty());
    std::unique_ptr<ICollection> values(table.getValuesProperty());
    std::vector<std::any> keySlots(3);
    std::vector<std::any> valueSlots(3);
    keys->CopyTo(keySlots, 0);
    values->CopyTo(valueSlots, 0);

    for (size_t i = 0; i < keySlots.size(); ++i) {
        const auto key = std::any_cast<std::string>(keySlots[i]);
        EXPECT_EQ(std::any_cast<std::string>(valueSlots[i]), "value-" + key)
            << "the key and value views must enumerate in the same order, as .NET guarantees";
    }
}

TEST(HashtableViews, ViewEnumeratorFailsFastAfterATableMutation) {
    Hashtable table;
    table.Add(std::string("a"), std::any(1));
    std::unique_ptr<ICollection> keys(table.getKeysProperty());
    std::unique_ptr<IEnumerator> walk(keys->GetEnumerator());

    table.Add(std::string("b"), std::any(2));
    EXPECT_THROW((void)walk->MoveNext(), System::InvalidOperationException);
}

TEST(HashtableViews, ViewEnumeratorCurrentIsTheKeyOrTheValue) {
    Hashtable table;
    table.Add(std::string("solo"), std::any(42));

    std::unique_ptr<ICollection> keys(table.getKeysProperty());
    std::unique_ptr<IEnumerator> keyWalk(keys->GetEnumerator());
    ASSERT_TRUE(keyWalk->MoveNext());
    EXPECT_EQ(std::any_cast<std::string>(keyWalk->getCurrentProperty()), "solo");

    std::unique_ptr<ICollection> values(table.getValuesProperty());
    std::unique_ptr<IEnumerator> valueWalk(values->GetEnumerator());
    ASSERT_TRUE(valueWalk->MoveNext());
    EXPECT_EQ(std::any_cast<int>(valueWalk->getCurrentProperty()), 42);
}

TEST(HashtableViews, AViewTracksTheTableItWasTakenFromNotACopy) {
    Hashtable original;
    original.Add(std::string("a"), std::any(1));
    std::unique_ptr<ICollection> keys(original.getKeysProperty());

    Hashtable copy = original;              // copy construction
    copy.Add(std::string("b"), std::any(2));

    EXPECT_EQ(keys->getCountProperty(), 1) << "the view follows the original, not the copy";
    EXPECT_EQ(copy.getCountProperty(), 2);

    std::unique_ptr<ICollection> copyKeys(copy.getKeysProperty());
    EXPECT_EQ(copyKeys->getCountProperty(), 2);
}

TEST(HashtableViews, ViewsOverAMoveConstructedTableAreUsable) {
    Hashtable source;
    source.Add(std::string("a"), std::any(1));
    source.Add(std::string("b"), std::any(2));

    Hashtable moved = std::move(source);
    std::unique_ptr<ICollection> keys(moved.getKeysProperty());
    ASSERT_NE(keys, nullptr);
    EXPECT_EQ(keys->getCountProperty(), 2);

    std::vector<std::any> destination(2);
    EXPECT_NO_THROW(keys->CopyTo(destination, 0));
}

TEST(HashtableViews, ViewsSurviveRepeatedTakeAndRelease) {
    Hashtable table;
    table.Add(std::string("a"), std::any(1));
    for (int i = 0; i < 32; ++i) {
        std::unique_ptr<ICollection> keys(table.getKeysProperty());
        std::unique_ptr<ICollection> values(table.getValuesProperty());
        ASSERT_EQ(keys->getCountProperty(), 1);
        ASSERT_EQ(values->getCountProperty(), 1);
    }
    EXPECT_EQ(table.getCountProperty(), 1);
}

TEST(HashtableViews, PolymorphicDispatchThroughIDictionaryMatchesTheConcreteCall) {
    Hashtable table;
    table.Add(std::string("a"), std::any(1));
    table.Add(std::string("b"), std::any(2));

    IDictionary& asInterface = table;
    std::unique_ptr<ICollection> throughInterface(asInterface.getKeysProperty());
    std::unique_ptr<ICollection> throughConcrete(table.getKeysProperty());

    ASSERT_NE(throughInterface, nullptr);
    ASSERT_NE(throughConcrete, nullptr);
    EXPECT_EQ(throughInterface->getCountProperty(), throughConcrete->getCountProperty());

    std::vector<std::any> viaInterface(2);
    std::vector<std::any> viaConcrete(2);
    throughInterface->CopyTo(viaInterface, 0);
    throughConcrete->CopyTo(viaConcrete, 0);
    for (size_t i = 0; i < viaInterface.size(); ++i)
        EXPECT_EQ(std::any_cast<std::string>(viaInterface[i]),
                  std::any_cast<std::string>(viaConcrete[i]));
}

TEST(HashtableViews, LargeTableViewCopiesEveryElement) {
    Hashtable table;
    constexpr int Count = 5000;
    for (int i = 0; i < Count; ++i)
        table.Add(std::string("k") + std::to_string(i), std::any(i));

    std::unique_ptr<ICollection> values(table.getValuesProperty());
    ASSERT_EQ(values->getCountProperty(), Count);

    std::vector<std::any> destination(Count);
    values->CopyTo(destination, 0);

    long long sum = 0;
    for (const auto& slot : destination) sum += std::any_cast<int>(slot);
    EXPECT_EQ(sum, static_cast<long long>(Count) * (Count - 1) / 2);
}

// The getKeys()/getValues() snapshot helpers are the documented alternative to
// the live views and must keep working unchanged.
TEST(HashtableViews, SnapshotHelpersRemainDetachedFromTheTable) {
    Hashtable table;
    table.Add(std::string("a"), std::any(1));
    std::vector<std::string> snapshot = table.getKeys();
    ASSERT_EQ(snapshot.size(), 1u);

    table.Add(std::string("b"), std::any(2));
    EXPECT_EQ(snapshot.size(), 1u) << "getKeys() is a snapshot, unlike getKeysProperty()";

    std::unique_ptr<ICollection> live(table.getKeysProperty());
    EXPECT_EQ(live->getCountProperty(), 2);
}

} // namespace
