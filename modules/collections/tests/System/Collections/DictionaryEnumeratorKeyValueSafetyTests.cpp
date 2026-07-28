// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1794
// (REMED-COLL-IDICTENUM-KEYVALUE-SAFETY), designed by ticket #1795 and recorded
// in docs/IDictionaryEnumeratorKeyValueSafetyDesign.md.
//
// Before this ticket, System::Collections::IDictionaryEnumerator's Key and Value
// accessors returned `const void*`, and neither implementation answered any
// accessor from state the enumerator owned. That produced seven distinct defect
// classes, of which these are the ones this suite pins shut:
//
//   A. Const-correctness bypass. Hashtable::Enumerator::getValueProperty()
//      returned &it_->second, a pointer to the live std::unordered_map's
//      mapped_type -- a NON-const std::any -- so const_cast plus assignment was
//      well-formed, fully defined C++ that rewrote live dictionary storage.
//      getKeyProperty() reached the const std::string key, where the same write
//      is undefined behaviour.
//   B. Mutation bypass. Neither write advanced the mutation counter, so a
//      second, outstanding enumerator saw nothing and kept enumerating.
//   C. Type safety. A `const void*` carries no type, size, or alignment, so a
//      wrong static_cast silently reinterpreted bytes; the same one-line helper
//      was correct against Hashtable and an AddressSanitizer
//      stack-buffer-overflow against ListDictionaryInternal.
//   D. Lifetime. NINE AddressSanitizer heap-use-after-free reports, three of
//      which needed no caller misuse at all: because no accessor version-checks,
//      an accessor called after a mutation dereferenced an invalidated container
//      iterator. On ListDictionaryInternal, which cached nothing, that reached
//      even getEntryProperty() and the already-owning getCurrentProperty().
//   E. Key integrity. At 64 entries a rewritten key left an entry that Count
//      still reported and no lookup could return by either its old or its new
//      key.
//   F/G. Interface inconsistency and implementation divergence: Entry and
//      Current owned while Key and Value borrowed, and the two implementations
//      disagreed about what Current even meant.
//
// The fix has two halves, and the second is the load-bearing one:
//   1. Key and Value return an owning std::any BY VALUE, equal by construction
//      to getEntryProperty()'s members;
//   2. every implementation snapshots the entry into enumerator-owned storage
//      during a successful MoveNext(), and NO accessor dereferences a container
//      iterator -- which is exactly what .NET's own HashtableEnumerator does.
//
// Cases that are about the INTERFACE are parameterised over both non-generic
// implementations, the shape ticket #1775's DictionaryKeyAndViewContractTests
// established, so neither implementation can regress alone. Cases that are about
// one implementation's own key or value domain are written against that type
// directly, because the two domains are genuinely different (Hashtable is
// string-hashed, ListDictionaryInternal is address-identity) and pretending
// otherwise would be a lie about one of them.
#include <gtest/gtest.h>

#include <any>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <typeinfo>
#include <type_traits>
#include <utility>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
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
// DIRECTLY is what lets "copy-out did not bump the version" be an assertion
// rather than an inference from a second enumerator's silence -- and the
// pre-#1794 defect was precisely that the counter did not move while live
// storage changed.
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

template<typename T>
using Owning = std::unique_ptr<T>;

// ---------------------------------------------------------------------------
// Harness: one uniform way to drive either non-generic IDictionary through the
// IDictionaryEnumerator interface, while still being able to answer questions
// that only that implementation's own key domain can answer.
// ---------------------------------------------------------------------------

class DictionaryHarness {
public:
    virtual ~DictionaryHarness() = default;

    virtual IDictionary& dictionary() = 0;
    /** @brief Adds @p count entries whose keys are all distinct and reachable. */
    virtual void addEntries(int count) = 0;
    /** @brief Adds one more entry, to move the mutation counter mid-enumeration. */
    virtual void addOneMore() = 0;
    /** @brief Removes the entry whose key @p boxedKey names. */
    virtual void removeEntryNamedBy(const std::any& boxedKey) = 0;
    /** @brief True when every key ever added is still reachable by lookup. */
    virtual bool allAddedKeysStillReachable() const = 0;
    /** @brief The mutation counter, read through the test-only seam. */
    virtual unsigned long long version() const = 0;

    /** @brief The type this implementation boxes into Key. */
    virtual const std::type_info& keyType() const = 0;
    /** @brief The type this implementation boxes into Value. */
    virtual const std::type_info& valueType() const = 0;

    /** @brief True when @p boxedKey is one of the keys this harness added. */
    virtual bool isOneOfOurKeys(const std::any& boxedKey) const = 0;
    /** @brief True when @p boxedValue is one of the values this harness added. */
    virtual bool isOneOfOurValues(const std::any& boxedValue) const = 0;

    virtual const char* name() const = 0;

    Owning<IDictionaryEnumerator> enumerator() {
        return Owning<IDictionaryEnumerator>(dictionary().GetEnumerator());
    }
};

class HashtableHarness : public DictionaryHarness {
    Hashtable table_;
    std::vector<std::string> keys_;
    std::vector<int> values_;
    int next_ = 0;

public:
    IDictionary& dictionary() override { return table_; }

    void addEntries(int count) override {
        for (int i = 0; i < count; ++i) addOneMore();
    }
    void addOneMore() override {
        keys_.push_back("key" + std::to_string(next_));
        values_.push_back(next_ * 10);
        table_.Add(keys_.back(), std::any(values_.back()));
        ++next_;
    }
    void removeEntryNamedBy(const std::any& boxedKey) override {
        table_.Remove(std::any_cast<std::string>(boxedKey));
    }
    bool allAddedKeysStillReachable() const override {
        for (const std::string& k : keys_)
            if (!table_.ContainsKey(k)) return false;
        return true;
    }
    unsigned long long version() const override {
        return SharpRuntime::Testing::CollectionVersionAccess<Hashtable>::version(table_);
    }

    const std::type_info& keyType() const override { return typeid(std::string); }
    const std::type_info& valueType() const override { return typeid(int); }

    bool isOneOfOurKeys(const std::any& boxedKey) const override {
        const auto* p = std::any_cast<std::string>(&boxedKey);
        if (p == nullptr) return false;
        for (const std::string& k : keys_) if (k == *p) return true;
        return false;
    }
    bool isOneOfOurValues(const std::any& boxedValue) const override {
        const auto* p = std::any_cast<int>(&boxedValue);
        if (p == nullptr) return false;
        for (int v : values_) if (v == *p) return true;
        return false;
    }
    const char* name() const override { return "Hashtable"; }

    Hashtable& table() { return table_; }
    const std::string& firstKey() const { return keys_.front(); }
};

class ListDictionaryInternalHarness : public DictionaryHarness {
    ListDictionaryInternal dictionary_;
    // Stable storage: this dictionary borrows the caller's key and value
    // addresses, and compares keys by address, so the backing store must not
    // reallocate. std::deque never invalidates references to existing elements.
    std::deque<int> keys_;
    std::deque<int> values_;

public:
    IDictionary& dictionary() override { return dictionary_; }

    void addEntries(int count) override {
        for (int i = 0; i < count; ++i) addOneMore();
    }
    void addOneMore() override {
        keys_.push_back(static_cast<int>(keys_.size()));
        values_.push_back(static_cast<int>(values_.size()) * 10);
        dictionary_.Add(static_cast<const void*>(&keys_.back()), &values_.back());
    }
    void removeEntryNamedBy(const std::any& boxedKey) override {
        dictionary_.Remove(std::any_cast<const void*>(boxedKey));
    }
    bool allAddedKeysStillReachable() const override {
        for (const int& k : keys_)
            if (!dictionary_.Contains(static_cast<const void*>(&k))) return false;
        return true;
    }
    unsigned long long version() const override {
        return SharpRuntime::Testing::CollectionVersionAccess<ListDictionaryInternal>::version(
            dictionary_);
    }

    const std::type_info& keyType() const override { return typeid(const void*); }
    const std::type_info& valueType() const override { return typeid(void*); }

    bool isOneOfOurKeys(const std::any& boxedKey) const override {
        const auto* p = std::any_cast<const void*>(&boxedKey);
        if (p == nullptr) return false;
        for (const int& k : keys_) if (static_cast<const void*>(&k) == *p) return true;
        return false;
    }
    bool isOneOfOurValues(const std::any& boxedValue) const override {
        const auto* p = std::any_cast<void*>(&boxedValue);
        if (p == nullptr) return false;
        for (const int& v : values_)
            if (static_cast<const void*>(&v) == static_cast<const void*>(*p)) return true;
        return false;
    }
    const char* name() const override { return "ListDictionaryInternal"; }
};

enum class Implementation { Hashtable, ListDictionaryInternal };

std::unique_ptr<DictionaryHarness> makeHarness(Implementation which) {
    if (which == Implementation::Hashtable) return std::make_unique<HashtableHarness>();
    return std::make_unique<ListDictionaryInternalHarness>();
}

/** @brief The exact .NET SR.InvalidOperation_EnumOpCantHappen text. */
constexpr const char* kEnumOpCantHappen =
    "Enumeration has either not started or has already finished.";
/** @brief The exact .NET SR.InvalidOperation_EnumFailedVersion text. */
constexpr const char* kEnumFailedVersion =
    "Collection was modified; enumeration operation may not execute.";

/** @brief Recovers the DictionaryEntry that getCurrentProperty() boxes. */
DictionaryEntry currentEntry(const IDictionaryEnumerator& e) {
    return std::any_cast<DictionaryEntry>(e.getCurrentProperty());
}

/**
 * @brief Compares two boxes by boxed type and by recovered value.
 *
 * std::any has no operator==, which is the same reason Generic::List<std::any>
 * cannot be instantiated (recorded by ticket #1793). The two shapes this suite
 * ever compares are the two the two implementations box.
 */
bool boxesAgree(const std::any& a, const std::any& b) {
    if (a.type() != b.type()) return false;
    if (!a.has_value()) return !b.has_value();
    if (a.type() == typeid(std::string))
        return std::any_cast<std::string>(a) == std::any_cast<std::string>(b);
    if (a.type() == typeid(int))
        return std::any_cast<int>(a) == std::any_cast<int>(b);
    if (a.type() == typeid(const void*))
        return std::any_cast<const void*>(a) == std::any_cast<const void*>(b);
    if (a.type() == typeid(void*))
        return std::any_cast<void*>(a) == std::any_cast<void*>(b);
    return false;
}

class DictionaryEnumeratorKeyValueSafety : public ::testing::TestWithParam<Implementation> {
protected:
    void SetUp() override { harness_ = makeHarness(GetParam()); }

    DictionaryHarness& harness() { return *harness_; }
    IDictionary& dictionary() { return harness_->dictionary(); }
    Owning<IDictionaryEnumerator> enumerator() { return harness_->enumerator(); }

private:
    std::unique_ptr<DictionaryHarness> harness_;
};

INSTANTIATE_TEST_SUITE_P(
    AllNonGenericDictionaries, DictionaryEnumeratorKeyValueSafety,
    ::testing::Values(Implementation::Hashtable, Implementation::ListDictionaryInternal),
    [](const ::testing::TestParamInfo<Implementation>& info) {
        return info.param == Implementation::Hashtable ? "Hashtable" : "ListDictionaryInternal";
    });

// ===========================================================================
// 1. The approved signatures themselves
// ===========================================================================

// The whole ticket is a return-type change on two public pure virtuals. If any
// of these four static_asserts ever fails, the ABI break described in
// README.md's breaking-changes entry has been undone or repeated silently.
TEST(DictionaryEnumeratorKeyValueContract, TheFourAccessorReturnTypesArePinned) {
    using E = const IDictionaryEnumerator&;
    static_assert(std::is_same_v<decltype(std::declval<E>().getEntryProperty()), DictionaryEntry>,
                  "Entry must return an owning DictionaryEntry BY VALUE");
    static_assert(std::is_same_v<decltype(std::declval<E>().getKeyProperty()), std::any>,
                  "Key must return an owning std::any BY VALUE, not const void*");
    static_assert(std::is_same_v<decltype(std::declval<E>().getValueProperty()), std::any>,
                  "Value must return an owning std::any BY VALUE, not const void*");
    static_assert(std::is_same_v<decltype(std::declval<E>().getCurrentProperty()), std::any>,
                  "Current keeps ticket #1793's owning std::any");
    SUCCEED();
}

// Every accessor is const-qualified and stays so: reading must never be a
// mutation, and a const enumerator reference must be able to answer all four.
TEST(DictionaryEnumeratorKeyValueContract, EveryAccessorIsConstQualified) {
    Hashtable table;
    table.Add(std::string("alpha"), std::any(1));
    Owning<IDictionaryEnumerator> mutableEnumerator(table.GetEnumerator());
    ASSERT_TRUE(mutableEnumerator->MoveNext());

    const IDictionaryEnumerator& e = *mutableEnumerator;
    EXPECT_NO_THROW((void)e.getEntryProperty());
    EXPECT_NO_THROW((void)e.getKeyProperty());
    EXPECT_NO_THROW((void)e.getValueProperty());
    EXPECT_NO_THROW((void)e.getCurrentProperty());
}

// ===========================================================================
// 2. Ownership -- writing to what came back cannot reach the dictionary
// ===========================================================================

TEST_P(DictionaryEnumeratorKeyValueSafety, MutatingTheReturnedKeyBoxCannotReachTheDictionary) {
    harness().addEntries(8);
    const intcs countBefore = dictionary().getCountProperty();
    const unsigned long long versionBefore = harness().version();

    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    std::any boxedKey = e->getKeyProperty();
    ASSERT_TRUE(harness().isOneOfOurKeys(boxedKey));

    // Every legal thing a caller can do to their own box.
    boxedKey = std::any(std::string("spoofed"));
    boxedKey.reset();
    boxedKey = std::any(42);

    EXPECT_EQ(dictionary().getCountProperty(), countBefore);
    EXPECT_TRUE(harness().allAddedKeysStillReachable());
    EXPECT_EQ(harness().version(), versionBefore) << "copy-out must not bump the counter";
}

TEST_P(DictionaryEnumeratorKeyValueSafety, MutatingTheReturnedValueBoxCannotReachTheDictionary) {
    harness().addEntries(8);
    const intcs countBefore = dictionary().getCountProperty();
    const unsigned long long versionBefore = harness().version();

    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    std::any boxedValue = e->getValueProperty();
    ASSERT_TRUE(harness().isOneOfOurValues(boxedValue));
    const std::any untouched = e->getValueProperty();

    boxedValue = std::any(std::string("rewritten through the enumerator"));

    // Re-reading the dictionary through a fresh enumerator still sees the
    // original value: pre-#1794, the equivalent write through the returned
    // `const void*` changed live Hashtable storage.
    auto verifier = enumerator();
    ASSERT_TRUE(verifier->MoveNext());
    EXPECT_TRUE(boxesAgree(verifier->getValueProperty(), untouched));

    EXPECT_EQ(dictionary().getCountProperty(), countBefore);
    EXPECT_EQ(harness().version(), versionBefore);
}

TEST_P(DictionaryEnumeratorKeyValueSafety, MutatingTheReturnedEntryCannotReachTheDictionary) {
    harness().addEntries(4);
    const intcs countBefore = dictionary().getCountProperty();
    const unsigned long long versionBefore = harness().version();

    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    DictionaryEntry entry = e->getEntryProperty();
    entry.setKeyProperty(std::any(std::string("spoofed")));
    entry.setValueProperty(std::any(-1));

    // The enumerator's own snapshot is unaffected too: it handed out a copy.
    EXPECT_TRUE(harness().isOneOfOurKeys(e->getKeyProperty()));
    EXPECT_EQ(dictionary().getCountProperty(), countBefore);
    EXPECT_TRUE(harness().allAddedKeysStillReachable());
    EXPECT_EQ(harness().version(), versionBefore);
}

// The boxed DictionaryEntry inside Current is likewise the caller's own; writing
// through a reference to it reaches neither the dictionary nor the snapshot.
TEST_P(DictionaryEnumeratorKeyValueSafety, MutatingTheBoxedCurrentEntryCannotReachTheDictionary) {
    harness().addEntries(4);
    const unsigned long long versionBefore = harness().version();

    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getCurrentProperty();
    ASSERT_EQ(boxed.type(), typeid(DictionaryEntry));
    auto& entry = std::any_cast<DictionaryEntry&>(boxed);
    entry = DictionaryEntry(std::any(std::string("spoofed")), std::any(999));

    EXPECT_TRUE(harness().isOneOfOurKeys(e->getKeyProperty()));
    EXPECT_TRUE(harness().allAddedKeysStillReachable());
    EXPECT_EQ(harness().version(), versionBefore);
}

// A second, outstanding enumerator is the observer the pre-#1794 write was
// invisible to. Copy-out is not a mutation, so it must stay valid -- this is the
// other half of "no artificial counter bump for a read".
TEST_P(DictionaryEnumeratorKeyValueSafety, CopyOutLeavesASecondEnumeratorValid) {
    harness().addEntries(4);

    auto first = enumerator();
    auto second = enumerator();
    ASSERT_TRUE(first->MoveNext());
    ASSERT_TRUE(second->MoveNext());

    for (int i = 0; i < 3; ++i) {
        (void)first->getKeyProperty();
        (void)first->getValueProperty();
        (void)first->getEntryProperty();
        (void)first->getCurrentProperty();
    }

    EXPECT_NO_THROW((void)second->MoveNext());
    EXPECT_NO_THROW(second->Reset());
    EXPECT_NO_THROW((void)first->MoveNext());
}

// ===========================================================================
// 3. Key integrity -- the section 8.1 A2 scenario, at the scale that exposed it
// ===========================================================================

// Pre-#1794 this scenario left an entry that Count still reported and that no
// lookup could return by EITHER its old or its new key: the enumerator's
// `const void*` aliased the live std::unordered_map key, and rewriting it broke
// the hash invariant without rehashing.
TEST(DictionaryEnumeratorKeyIntegrity, SixtyFourEntryHashtableSurvivesEveryLegalBoxOperation) {
    Hashtable table;
    for (int i = 0; i < 64; ++i)
        table.Add("key" + std::to_string(i), std::any(i));
    const unsigned long long versionBefore =
        SharpRuntime::Testing::CollectionVersionAccess<Hashtable>::version(table);

    Owning<IDictionaryEnumerator> e(table.GetEnumerator());
    while (e->MoveNext()) {
        std::any boxedKey = e->getKeyProperty();
        ASSERT_EQ(boxedKey.type(), typeid(std::string));
        // Mutate the caller's own copy in every way the type allows.
        auto& key = std::any_cast<std::string&>(boxedKey);
        key = "CORRUPTED";
        key.clear();
        boxedKey.reset();
    }

    EXPECT_EQ(table.getCountProperty(), 64);
    for (int i = 0; i < 64; ++i) {
        const std::string k = "key" + std::to_string(i);
        EXPECT_TRUE(table.ContainsKey(k)) << "entry " << k << " became unreachable";
        EXPECT_EQ(std::any_cast<int>(table.at(k)), i);
    }
    EXPECT_FALSE(table.ContainsKey("CORRUPTED"));
    EXPECT_EQ(SharpRuntime::Testing::CollectionVersionAccess<Hashtable>::version(table),
              versionBefore);
}

// ListDictionaryInternal compares keys by ADDRESS, so its key box holds a
// pointer the caller already owns. Replacing that pointer inside the box must
// not replace the dictionary's entry -- pointer semantics, not deep copy.
TEST(DictionaryEnumeratorKeyIntegrity, ListDictionaryInternalKeyBoxIsAHandleNotAnEntryAlias) {
    ListDictionaryInternal dictionary;
    const int key = 1;
    const int otherKey = 2;
    int value = 99;
    dictionary.Add(&key, &value);

    Owning<IDictionaryEnumerator> e(dictionary.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxedKey = e->getKeyProperty();
    ASSERT_EQ(std::any_cast<const void*>(boxedKey), static_cast<const void*>(&key));
    std::any_cast<const void*&>(boxedKey) = static_cast<const void*>(&otherKey);

    EXPECT_EQ(dictionary.getCountProperty(), 1);
    EXPECT_TRUE(dictionary.Contains(&key));
    EXPECT_FALSE(dictionary.Contains(&otherKey));
    EXPECT_EQ(dictionary.getItem(&key), static_cast<void*>(&value));
}

// ===========================================================================
// 4. Type safety -- the class a `const void*` could not express at all
// ===========================================================================

TEST_P(DictionaryEnumeratorKeyValueSafety, TheBoxedTypeIsTheDocumentedOnePerImplementation) {
    harness().addEntries(1);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    EXPECT_EQ(e->getKeyProperty().type(), harness().keyType());
    EXPECT_EQ(e->getValueProperty().type(), harness().valueType());
    EXPECT_EQ(e->getCurrentProperty().type(), typeid(DictionaryEntry));
    EXPECT_EQ(e->getEntryProperty().getKeyProperty().type(), harness().keyType());
    EXPECT_EQ(e->getEntryProperty().getValueProperty().type(), harness().valueType());
}

TEST_P(DictionaryEnumeratorKeyValueSafety, AWrongCastThrowsBadAnyCastInsteadOfReinterpreting) {
    harness().addEntries(1);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    // Pre-#1794 each of these was a silent reinterpretation of raw bytes, and
    // one of them was an AddressSanitizer stack-buffer-overflow.
    EXPECT_THROW((void)std::any_cast<double>(e->getKeyProperty()), std::bad_any_cast);
    EXPECT_THROW((void)std::any_cast<double>(e->getValueProperty()), std::bad_any_cast);
    EXPECT_THROW((void)std::any_cast<std::string>(e->getCurrentProperty()), std::bad_any_cast);

    // The pointer form is the checked, non-throwing spelling of the same query.
    const DictionaryEntry entry = e->getEntryProperty();
    EXPECT_EQ(std::any_cast<double>(&entry.getKeyProperty()), nullptr);
}

// The one-line helper that was correct against one implementation and a
// stack-buffer-overflow against the other is now diagnosable through the
// interface, without the caller knowing which implementation it holds.
TEST_P(DictionaryEnumeratorKeyValueSafety, CrossImplementationCodeIsDiagnosedNotCorrupted) {
    harness().addEntries(1);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    const std::any boxedKey = e->getKeyProperty();
    if (boxedKey.type() == typeid(std::string)) {
        EXPECT_NO_THROW((void)std::any_cast<std::string>(boxedKey));
        EXPECT_THROW((void)std::any_cast<const void*>(boxedKey), std::bad_any_cast);
    } else {
        EXPECT_NO_THROW((void)std::any_cast<const void*>(boxedKey));
        EXPECT_THROW((void)std::any_cast<std::string>(boxedKey), std::bad_any_cast);
    }
    // Either way the runtime type is queryable, which a const void* never was.
    EXPECT_NE(boxedKey.type(), typeid(void));
}

// ===========================================================================
// 5. Lifetime -- the nine AddressSanitizer reports of design section 8.2, flipped
// ===========================================================================

TEST_P(DictionaryEnumeratorKeyValueSafety, ReturnedBoxesSurviveMoveNext) {
    harness().addEntries(4);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    const std::any key = e->getKeyProperty();
    const std::any value = e->getValueProperty();
    const DictionaryEntry entry = e->getEntryProperty();
    const std::any current = e->getCurrentProperty();

    ASSERT_TRUE(e->MoveNext());

    EXPECT_TRUE(harness().isOneOfOurKeys(key));
    EXPECT_TRUE(harness().isOneOfOurValues(value));
    EXPECT_TRUE(boxesAgree(entry.getKeyProperty(), key));
    EXPECT_TRUE(boxesAgree(std::any_cast<DictionaryEntry>(current).getKeyProperty(), key));
    // ...and they still name the PREVIOUS entry, not the current one: the box is
    // a snapshot, so it cannot silently rename itself the way a retained
    // `const void*` into map storage did.
    EXPECT_FALSE(boxesAgree(e->getKeyProperty(), key));
}

TEST_P(DictionaryEnumeratorKeyValueSafety, ReturnedBoxesSurviveReset) {
    harness().addEntries(4);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    const std::any key = e->getKeyProperty();
    const std::any value = e->getValueProperty();
    e->Reset();

    EXPECT_TRUE(harness().isOneOfOurKeys(key));
    EXPECT_TRUE(harness().isOneOfOurValues(value));
}

TEST_P(DictionaryEnumeratorKeyValueSafety, ReturnedBoxesSurviveAddAndRehash) {
    harness().addEntries(4);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    const std::any key = e->getKeyProperty();
    const std::any value = e->getValueProperty();
    for (int i = 0; i < 300; ++i) harness().addOneMore();

    EXPECT_TRUE(harness().isOneOfOurKeys(key));
    EXPECT_TRUE(harness().isOneOfOurValues(value));
}

TEST_P(DictionaryEnumeratorKeyValueSafety, ReturnedBoxesSurviveRemovalOfTheirOwnEntry) {
    harness().addEntries(4);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    const std::any key = e->getKeyProperty();
    const std::any value = e->getValueProperty();
    const DictionaryEntry entry = e->getEntryProperty();
    harness().removeEntryNamedBy(key);

    EXPECT_TRUE(key.has_value());
    EXPECT_EQ(key.type(), harness().keyType());
    EXPECT_TRUE(value.has_value());
    EXPECT_TRUE(boxesAgree(entry.getKeyProperty(), key));
}

TEST_P(DictionaryEnumeratorKeyValueSafety, ReturnedBoxesSurviveClear) {
    harness().addEntries(4);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    const std::any key = e->getKeyProperty();
    const std::any value = e->getValueProperty();
    const DictionaryEntry entry = e->getEntryProperty();
    const std::any current = e->getCurrentProperty();
    dictionary().Clear();

    EXPECT_EQ(key.type(), harness().keyType());
    EXPECT_EQ(value.type(), harness().valueType());
    EXPECT_TRUE(boxesAgree(entry.getKeyProperty(), key));
    EXPECT_TRUE(boxesAgree(std::any_cast<DictionaryEntry>(current).getValueProperty(), value));
    EXPECT_EQ(dictionary().getCountProperty(), 0);
}

TEST_P(DictionaryEnumeratorKeyValueSafety, ReturnedBoxesSurviveEnumeratorDestruction) {
    harness().addEntries(4);

    std::any key;
    std::any value;
    DictionaryEntry entry;
    std::any current;
    {
        auto e = enumerator();
        ASSERT_TRUE(e->MoveNext());
        key = e->getKeyProperty();
        value = e->getValueProperty();
        entry = e->getEntryProperty();
        current = e->getCurrentProperty();
    } // the enumerator is destroyed here

    EXPECT_TRUE(harness().isOneOfOurKeys(key));
    EXPECT_TRUE(harness().isOneOfOurValues(value));
    EXPECT_TRUE(boxesAgree(entry.getKeyProperty(), key));
    EXPECT_TRUE(boxesAgree(std::any_cast<DictionaryEntry>(current).getKeyProperty(), key));
}

// The collection outlives nothing here: the boxes do. This is the scenario that
// was an AddressSanitizer heap-use-after-free on BOTH implementations before
// #1794, and on ListDictionaryInternal reached even Entry and Current.
TEST(DictionaryEnumeratorLifetime, HashtableBoxesSurviveTheDictionarysDestruction) {
    std::any key;
    std::any value;
    DictionaryEntry entry;
    std::any current;
    {
        Hashtable table;
        table.Add(std::string("alpha"), std::any(7));
        Owning<IDictionaryEnumerator> e(table.GetEnumerator());
        ASSERT_TRUE(e->MoveNext());
        key = e->getKeyProperty();
        value = e->getValueProperty();
        entry = e->getEntryProperty();
        current = e->getCurrentProperty();
    } // enumerator AND table destroyed

    EXPECT_EQ(std::any_cast<std::string>(key), "alpha");
    EXPECT_EQ(std::any_cast<int>(value), 7);
    EXPECT_EQ(std::any_cast<std::string>(entry.getKeyProperty()), "alpha");
    EXPECT_EQ(std::any_cast<int>(
                  std::any_cast<DictionaryEntry>(current).getValueProperty()),
              7);
}

TEST(DictionaryEnumeratorLifetime, ListDictionaryInternalBoxesSurviveTheDictionarysDestruction) {
    const int key = 1;
    int value = 99;

    std::any boxedKey;
    std::any boxedValue;
    DictionaryEntry entry;
    std::any current;
    {
        ListDictionaryInternal dictionary;
        dictionary.Add(&key, &value);
        Owning<IDictionaryEnumerator> e(dictionary.GetEnumerator());
        ASSERT_TRUE(e->MoveNext());
        boxedKey = e->getKeyProperty();
        boxedValue = e->getValueProperty();
        entry = e->getEntryProperty();
        current = e->getCurrentProperty();
    } // enumerator AND dictionary destroyed

    EXPECT_EQ(std::any_cast<const void*>(boxedKey), static_cast<const void*>(&key));
    EXPECT_EQ(std::any_cast<void*>(boxedValue), static_cast<void*>(&value));
    EXPECT_EQ(std::any_cast<const void*>(entry.getKeyProperty()),
              static_cast<const void*>(&key));
    EXPECT_EQ(std::any_cast<void*>(
                  std::any_cast<DictionaryEntry>(current).getValueProperty()),
              static_cast<void*>(&value));
}

// ===========================================================================
// 6. Accessor-after-mutation -- three of the nine reports needed NO caller misuse
// ===========================================================================

// Calling an accessor AGAIN after the dictionary was cleared must return the
// snapshot. Pre-#1794 this dereferenced an invalidated container iterator on
// both implementations, and no accessor version-checks, so nothing diagnosed it.
TEST_P(DictionaryEnumeratorKeyValueSafety, EveryAccessorAfterClearReturnsTheSnapshot) {
    harness().addEntries(4);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    const std::any keyBefore = e->getKeyProperty();
    const std::any valueBefore = e->getValueProperty();

    dictionary().Clear();

    EXPECT_TRUE(boxesAgree(e->getKeyProperty(), keyBefore));
    EXPECT_TRUE(boxesAgree(e->getValueProperty(), valueBefore));
    EXPECT_TRUE(boxesAgree(e->getEntryProperty().getKeyProperty(), keyBefore));
    EXPECT_TRUE(boxesAgree(currentEntry(*e).getKeyProperty(), keyBefore));
    EXPECT_TRUE(boxesAgree(currentEntry(*e).getValueProperty(), valueBefore));
}

TEST_P(DictionaryEnumeratorKeyValueSafety, EveryAccessorAfterRemovalReturnsTheSnapshot) {
    harness().addEntries(4);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    const std::any keyBefore = e->getKeyProperty();
    const std::any valueBefore = e->getValueProperty();

    harness().removeEntryNamedBy(keyBefore);

    EXPECT_TRUE(boxesAgree(e->getKeyProperty(), keyBefore));
    EXPECT_TRUE(boxesAgree(e->getValueProperty(), valueBefore));
    EXPECT_TRUE(boxesAgree(e->getEntryProperty().getKeyProperty(), keyBefore));
    EXPECT_TRUE(boxesAgree(currentEntry(*e).getKeyProperty(), keyBefore));
}

// Repeated reads at one position are independent copies, not aliases of one
// another, and are equivalent every time.
TEST_P(DictionaryEnumeratorKeyValueSafety, RepeatedAccessAtOnePositionYieldsIndependentCopies) {
    harness().addEntries(4);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    std::any first = e->getKeyProperty();
    std::any second = e->getKeyProperty();
    EXPECT_TRUE(boxesAgree(first, second));

    first.reset();                                   // destroying one copy...
    EXPECT_TRUE(harness().isOneOfOurKeys(second));    // ...leaves the other intact
    EXPECT_TRUE(harness().isOneOfOurKeys(e->getKeyProperty()));

    DictionaryEntry entryA = e->getEntryProperty();
    DictionaryEntry entryB = e->getEntryProperty();
    entryA.setValueProperty(std::any(-1));
    EXPECT_TRUE(harness().isOneOfOurValues(entryB.getValueProperty()));
}

// ===========================================================================
// 7. Entry / Current / Key / Value agreement -- design section 15's invariants
// ===========================================================================

TEST_P(DictionaryEnumeratorKeyValueSafety, TheFourAccessorsAgreeAtEveryPosition) {
    harness().addEntries(6);
    auto e = enumerator();

    int seen = 0;
    while (e->MoveNext()) {
        ++seen;
        const DictionaryEntry entry = e->getEntryProperty();
        const std::any key = e->getKeyProperty();
        const std::any value = e->getValueProperty();
        const DictionaryEntry boxed = currentEntry(*e);

        EXPECT_TRUE(boxesAgree(entry.getKeyProperty(), key));
        EXPECT_TRUE(boxesAgree(entry.getValueProperty(), value));
        EXPECT_TRUE(boxesAgree(boxed.getKeyProperty(), key));
        EXPECT_TRUE(boxesAgree(boxed.getValueProperty(), value));
    }
    EXPECT_EQ(seen, 6);
}

// .NET is `public object Current => Entry;` on BOTH of its implementations.
// ListDictionaryInternal boxed only the key until #1794, so one interface had
// two incompatible meanings for Current.
TEST_P(DictionaryEnumeratorKeyValueSafety, CurrentBoxesTheEntryOnEveryImplementation) {
    harness().addEntries(2);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());

    const std::any current = e->getCurrentProperty();
    ASSERT_EQ(current.type(), typeid(DictionaryEntry))
        << "Current must be Entry boxed, matching .NET's `Current => Entry`";
    EXPECT_NE(current.type(), harness().keyType());
}

// ===========================================================================
// 8. No nested std::any -- a property of the standard library's overload set
// ===========================================================================

// Hashtable's mapped_type IS std::any. std::any(const std::any&) selects the
// copy constructor rather than the value-forwarding one, so the box holds the
// PAYLOAD and one std::any_cast recovers the stored value.
TEST(DictionaryEnumeratorBoxing, HashtableValuesAreNeverNestedInsideASecondAny) {
    Hashtable table;
    table.Add(std::string("int"), std::any(1234));
    table.Add(std::string("string"), std::any(std::string("payload")));
    table.Add(std::string("entry"),
              std::any(DictionaryEntry(std::any(1), std::any(2))));

    Owning<IDictionaryEnumerator> e(table.GetEnumerator());
    int seen = 0;
    while (e->MoveNext()) {
        ++seen;
        const std::any value = e->getValueProperty();
        EXPECT_NE(value.type(), typeid(std::any)) << "the box must not wrap a std::any";

        const std::string key = std::any_cast<std::string>(e->getKeyProperty());
        if (key == "int") {
            EXPECT_EQ(value.type(), typeid(int));
            EXPECT_EQ(std::any_cast<int>(value), 1234);
        } else if (key == "string") {
            EXPECT_EQ(value.type(), typeid(std::string));
            EXPECT_EQ(std::any_cast<std::string>(value), "payload");
        } else {
            EXPECT_EQ(value.type(), typeid(DictionaryEntry));
        }
        // The Entry member and the value view agree with it.
        EXPECT_EQ(e->getEntryProperty().getValueProperty().type(), value.type());
    }
    EXPECT_EQ(seen, 3);

    Owning<ICollection> values(table.getValuesProperty());
    Owning<IEnumerator> walk(values->GetEnumerator());
    while (walk->MoveNext())
        EXPECT_NE(walk->getCurrentProperty().type(), typeid(std::any));
}

// ===========================================================================
// 9. Element type behaviour, including the pointer-valued cases
// ===========================================================================

namespace {
/** @brief Counts its own copies and destructions, to pin what an accessor costs. */
struct Counted {
    static int liveInstances;
    static int copies;
    int payload = 0;

    explicit Counted(int p) : payload(p) { ++liveInstances; }
    Counted(const Counted& other) : payload(other.payload) { ++liveInstances; ++copies; }
    Counted& operator=(const Counted& other) { payload = other.payload; ++copies; return *this; }
    ~Counted() { --liveInstances; }
};
int Counted::liveInstances = 0;
int Counted::copies = 0;
} // namespace

TEST(DictionaryEnumeratorElementTypes, NonTrivialValuesAreCopiedOutAndFullyReleased) {
    Counted::liveInstances = 0;
    Counted::copies = 0;
    {
        Hashtable table;
        table.Add(std::string("counted"), std::any(Counted(5)));

        Owning<IDictionaryEnumerator> e(table.GetEnumerator());
        ASSERT_TRUE(e->MoveNext());

        const int copiesBeforeAccess = Counted::copies;
        const std::any value = e->getValueProperty();
        ASSERT_EQ(value.type(), typeid(Counted));
        EXPECT_EQ(std::any_cast<const Counted&>(value).payload, 5);
        EXPECT_GT(Counted::copies, copiesBeforeAccess)
            << "the accessor must hand back a copy, not an alias";
    }
    EXPECT_EQ(Counted::liveInstances, 0) << "every copy the accessor made must be released";
}

// A raw pointer VALUE: the box copies the pointer, not the pointee. Mutating the
// pointee through the copied pointer is naturally visible -- that is pointer
// semantics, not a defect, and the same aliasing .NET has for a reference type.
// Replacing the pointer inside the box replaces nothing in the dictionary.
TEST(DictionaryEnumeratorElementTypes, RawPointerValuesKeepPointerSemantics) {
    int target = 1;
    int other = 2;
    Hashtable table;
    table.Add(std::string("ptr"), std::any(&target));

    Owning<IDictionaryEnumerator> e(table.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    std::any boxed = e->getValueProperty();
    ASSERT_EQ(boxed.type(), typeid(int*));
    int* recovered = std::any_cast<int*>(boxed);
    EXPECT_EQ(recovered, &target);

    *recovered = 42;                       // reaches the POINTEE, which the caller owns
    EXPECT_EQ(target, 42);

    std::any_cast<int*&>(boxed) = &other;  // replaces only the caller's handle
    EXPECT_EQ(std::any_cast<int*>(table.at("ptr")), &target);
    EXPECT_EQ(table.getCountProperty(), 1);
}

TEST(DictionaryEnumeratorElementTypes, SharedPtrValuesCopyTheHandleAndKeepTheUseCount) {
    auto shared = std::make_shared<int>(7);
    EXPECT_EQ(shared.use_count(), 1);

    Hashtable table;
    table.Add(std::string("shared"), std::any(shared));
    const long afterInsert = shared.use_count();
    EXPECT_GT(afterInsert, 1);

    {
        Owning<IDictionaryEnumerator> e(table.GetEnumerator());
        ASSERT_TRUE(e->MoveNext());

        const std::any boxed = e->getValueProperty();
        ASSERT_EQ(boxed.type(), typeid(std::shared_ptr<int>));
        auto recovered = std::any_cast<std::shared_ptr<int>>(boxed);
        EXPECT_EQ(recovered.get(), shared.get());
        EXPECT_GT(shared.use_count(), afterInsert) << "the box owns a share";
        *recovered = 11;                            // the POINTEE is shared, by design
    }
    EXPECT_EQ(*shared, 11);
    EXPECT_EQ(shared.use_count(), afterInsert) << "the box released its share";
    EXPECT_EQ(table.getCountProperty(), 1);
}

// .NET's Value is `object?`, and a null value is a null reference. This port
// stores an empty std::any, and the accessor must hand back an empty box rather
// than throwing or fabricating a value.
TEST(DictionaryEnumeratorElementTypes, ANullValueBoxesAsAnEmptyAny) {
    Hashtable table;
    table.Add(std::string("null"), std::any{});

    Owning<IDictionaryEnumerator> e(table.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    const std::any value = e->getValueProperty();
    EXPECT_FALSE(value.has_value());
    EXPECT_FALSE(e->getEntryProperty().getValueProperty().has_value());
    EXPECT_FALSE(currentEntry(*e).getValueProperty().has_value());
    EXPECT_TRUE(e->getKeyProperty().has_value()) << ".NET's Key is non-nullable";
}

TEST(DictionaryEnumeratorElementTypes, PrimitiveAndStringValuesRoundTripThroughTheBox) {
    Hashtable table;
    table.Add(std::string("i"), std::any(-3));
    table.Add(std::string("d"), std::any(2.5));
    table.Add(std::string("b"), std::any(true));
    table.Add(std::string("s"), std::any(std::string("text")));

    Owning<IDictionaryEnumerator> e(table.GetEnumerator());
    int seen = 0;
    while (e->MoveNext()) {
        ++seen;
        const std::string key = std::any_cast<std::string>(e->getKeyProperty());
        const std::any value = e->getValueProperty();
        if (key == "i") EXPECT_EQ(std::any_cast<int>(value), -3);
        else if (key == "d") EXPECT_DOUBLE_EQ(std::any_cast<double>(value), 2.5);
        else if (key == "b") EXPECT_TRUE(std::any_cast<bool>(value));
        else EXPECT_EQ(std::any_cast<std::string>(value), "text");
    }
    EXPECT_EQ(seen, 4);
}

// ===========================================================================
// 10. State machine and exception behaviour -- unchanged in every cell
// ===========================================================================

TEST_P(DictionaryEnumeratorKeyValueSafety, EveryAccessorThrowsBeforeTheFirstMoveNext) {
    harness().addEntries(3);
    auto e = enumerator();

    for (const auto& call : std::vector<std::function<void()>>{
             [&] { (void)e->getEntryProperty(); },
             [&] { (void)e->getKeyProperty(); },
             [&] { (void)e->getValueProperty(); },
             [&] { (void)e->getCurrentProperty(); }}) {
        try {
            call();
            ADD_FAILURE() << "expected InvalidOperationException before the first MoveNext";
        } catch (const System::InvalidOperationException& ex) {
            EXPECT_STREQ(ex.what(), kEnumOpCantHappen);
        }
    }
}

TEST_P(DictionaryEnumeratorKeyValueSafety, EveryAccessorThrowsAfterTheLastElement) {
    harness().addEntries(3);
    auto e = enumerator();
    while (e->MoveNext()) { }

    for (const auto& call : std::vector<std::function<void()>>{
             [&] { (void)e->getEntryProperty(); },
             [&] { (void)e->getKeyProperty(); },
             [&] { (void)e->getValueProperty(); },
             [&] { (void)e->getCurrentProperty(); }}) {
        try {
            call();
            ADD_FAILURE() << "expected InvalidOperationException after the last element";
        } catch (const System::InvalidOperationException& ex) {
            EXPECT_STREQ(ex.what(), kEnumOpCantHappen);
        }
    }
}

TEST_P(DictionaryEnumeratorKeyValueSafety, EveryAccessorThrowsAfterReset) {
    harness().addEntries(3);
    auto e = enumerator();
    ASSERT_TRUE(e->MoveNext());
    e->Reset();

    EXPECT_THROW((void)e->getEntryProperty(), System::InvalidOperationException);
    EXPECT_THROW((void)e->getKeyProperty(), System::InvalidOperationException);
    EXPECT_THROW((void)e->getValueProperty(), System::InvalidOperationException);
    EXPECT_THROW((void)e->getCurrentProperty(), System::InvalidOperationException);
}

// The state check must run BEFORE any copy is made or any box constructed, so
// that no exception path changed type or order under #1794 -- the ordering rule
// ticket #1793 set and whose violation it had to repair.
TEST_P(DictionaryEnumeratorKeyValueSafety, TheStateCheckPrecedesAnyCopy) {
    harness().addEntries(1);
    auto e = enumerator();
    const unsigned long long versionBefore = harness().version();

    EXPECT_THROW((void)e->getKeyProperty(), System::InvalidOperationException);
    EXPECT_EQ(harness().version(), versionBefore);
    EXPECT_EQ(dictionary().getCountProperty(), 1);
}

// A real structural mutation still invalidates MoveNext()/Reset(), with .NET's
// exact message. #1794 adds NO version check to the accessors, deliberately:
// .NET's do not have one either, and the snapshot makes the stale read safe.
TEST_P(DictionaryEnumeratorKeyValueSafety, RealMutationStillFailsFastOnMoveNextAndReset) {
    harness().addEntries(3);

    auto forMoveNext = enumerator();
    auto forReset = enumerator();
    ASSERT_TRUE(forMoveNext->MoveNext());
    ASSERT_TRUE(forReset->MoveNext());

    harness().addOneMore();

    try {
        (void)forMoveNext->MoveNext();
        ADD_FAILURE() << "MoveNext must fail fast after a structural mutation";
    } catch (const System::InvalidOperationException& ex) {
        EXPECT_STREQ(ex.what(), kEnumFailedVersion);
    }
    try {
        forReset->Reset();
        ADD_FAILURE() << "Reset must fail fast after a structural mutation";
    } catch (const System::InvalidOperationException& ex) {
        EXPECT_STREQ(ex.what(), kEnumFailedVersion);
    }
    // ...while the accessors keep answering from the snapshot.
    EXPECT_NO_THROW((void)forMoveNext->getKeyProperty());
    EXPECT_NO_THROW((void)forMoveNext->getValueProperty());
}

// ===========================================================================
// 11. Member views -- the four element shapes of design section 14.3
// ===========================================================================

TEST(DictionaryEnumeratorMemberViews, HashtableViewElementShapesAreUnchanged) {
    Hashtable table;
    table.Add(std::string("alpha"), std::any(1));

    Owning<ICollection> keys(table.getKeysProperty());
    Owning<IEnumerator> keyWalk(keys->GetEnumerator());
    ASSERT_TRUE(keyWalk->MoveNext());
    const std::any boxedKey = keyWalk->getCurrentProperty();
    EXPECT_EQ(boxedKey.type(), typeid(std::string));
    EXPECT_EQ(std::any_cast<std::string>(boxedKey), "alpha");

    Owning<ICollection> values(table.getValuesProperty());
    Owning<IEnumerator> valueWalk(values->GetEnumerator());
    ASSERT_TRUE(valueWalk->MoveNext());
    const std::any boxedValue = valueWalk->getCurrentProperty();
    EXPECT_EQ(boxedValue.type(), typeid(int)) << "the value view boxes the payload, not a std::any";
    EXPECT_EQ(std::any_cast<int>(boxedValue), 1);
}

// The key view still boxes `const void*`; the VALUE view now boxes `void*`,
// agreeing with DictionaryEntry::Value and copyToCore where it previously
// agreed with neither. This is the deliberate behaviour change of the approval.
TEST(DictionaryEnumeratorMemberViews, ListDictionaryInternalValueViewBoxesNonConstVoidPointer) {
    ListDictionaryInternal dictionary;
    const int key = 1;
    int value = 99;
    dictionary.Add(&key, &value);

    Owning<ICollection> keys(dictionary.getKeysProperty());
    Owning<IEnumerator> keyWalk(keys->GetEnumerator());
    ASSERT_TRUE(keyWalk->MoveNext());
    const std::any boxedKey = keyWalk->getCurrentProperty();
    EXPECT_EQ(boxedKey.type(), typeid(const void*));
    EXPECT_EQ(std::any_cast<const void*>(boxedKey), static_cast<const void*>(&key));

    Owning<ICollection> values(dictionary.getValuesProperty());
    Owning<IEnumerator> valueWalk(values->GetEnumerator());
    ASSERT_TRUE(valueWalk->MoveNext());
    const std::any boxedValue = valueWalk->getCurrentProperty();
    EXPECT_EQ(boxedValue.type(), typeid(void*));
    EXPECT_EQ(std::any_cast<void*>(boxedValue), static_cast<void*>(&value));
    EXPECT_THROW((void)std::any_cast<const void*>(boxedValue), std::bad_any_cast);
}

// Both member views forward the boxed accessors, so a view element outlives the
// view, its inner enumerator, and the dictionary itself.
TEST_P(DictionaryEnumeratorKeyValueSafety, ViewElementsAreOwningCopiesToo) {
    harness().addEntries(3);
    const unsigned long long versionBefore = harness().version();

    std::any firstKey;
    {
        Owning<ICollection> keys(dictionary().getKeysProperty());
        Owning<IEnumerator> walk(keys->GetEnumerator());
        ASSERT_TRUE(walk->MoveNext());
        firstKey = walk->getCurrentProperty();
    } // view and its inner enumerator destroyed

    EXPECT_TRUE(harness().isOneOfOurKeys(firstKey));
    dictionary().Clear();
    EXPECT_EQ(firstKey.type(), harness().keyType());
    EXPECT_EQ(harness().version(), versionBefore + 1) << "only the Clear moved the counter";
}

} // namespace
