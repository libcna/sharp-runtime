// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1796
// (REMED-COLL-HASHTABLE-WRITE-ESCAPES), designed by ticket #1797 and recorded in
// docs/HashtableValueAccessSafetyDesign.md.
//
// Before this ticket System::Collections::Hashtable had FOUR routes by which a
// caller could obtain, or write through, something that aliased live value
// storage -- not the two the ticket was named after:
//
//   1. void* getItem(const void*) const        -- a CONST member returning a
//      writable pointer into the live std::unordered_map. Reachable from a
//      `const Hashtable&` and from a `const IDictionary&`, so even the most
//      restrictive reference the interface offered was a write path.
//   2. std::any& operator[](const std::string&) -- a mutable reference into
//      storage, and, because it forwarded to std::unordered_map::operator[], a
//      bare READ of an absent key performed a STRUCTURAL insert.
//   3. const std::any& at(const std::string&) const -- a const alias to a
//      NON-const std::any, so `const_cast<std::any&>(h.at(k)) = v` was
//      well-formed, fully defined C++ that rewrote the dictionary.
//   4. setItem/Add's raw `void*` VALUE parameter -- an input-side type hole,
//      deliberately RETAINED (design section 13.4) because migrating it to
//      `const std::any&` silently stores `Add("literal", v)` under the
//      stringified ADDRESS of the literal.
//
// Nine of those routes' fourteen measured lifetime scenarios were
// AddressSanitizer heap-use-after-free reports. The single worst case produced
// no report from any sanitizer at all: an untracked structural insert through
// operator[] left an outstanding enumerator's version check passing, and at
// 4,008 entries it walked 2,045 distinct keys, reached only 6 of its 8
// pre-mutation seed keys, and threw nothing. Memory-safe and wrong.
//
// The fix is three rules, and every case below pins one of them:
//   A. EVERY read returns an owning std::any BY VALUE -- .NET's "independent
//      handle to the value object", which in C++ only a copy reproduces.
//   B. EVERY write goes through a member that owns the mutation counter, so
//      insert, replace AND equal-replace all invalidate outstanding enumerators
//      (matching .NET Hashtable.Insert, which calls UpdateVersion() on both
//      branches and never compares the old value).
//   C. A read NEVER inserts.
//
// Cases about the IDictionary INTERFACE are parameterised over both non-generic
// implementations, the shape ticket #1775 established, so neither can regress
// alone. Cases about Hashtable's own string-keyed value domain are written
// against Hashtable directly, because ListDictionaryInternal's domain is
// address-identity and pretending otherwise would be a lie about one of them.
//
// ListDictionaryInternal's OWN defects -- setItem skipping the version bump on
// the replace branch, and both accessors accepting a null key where .NET and
// Hashtable throw -- are ticket #1798 and are deliberately NOT fixed here. Where
// a case below would otherwise assert across both implementations and cannot,
// that is said in place rather than quietly skipped.
#include <gtest/gtest.h>

#include <any>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/Exception.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Collections/DictionaryEntry.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"
#include "System/Collections/detail/MutationCounter.hpp"

using SharpRuntime::intcs;
using System::Collections::DictionaryEntry;
using System::Collections::Hashtable;
using System::Collections::ICollection;
using System::Collections::IDictionary;
using System::Collections::IDictionaryEnumerator;
using System::Collections::IEnumerator;
using System::Collections::ListDictionaryInternal;
using System::Collections::Generic::KeyNotFoundException;

// ---------------------------------------------------------------------------
// Mutation-counter seam. Declared by detail/MutationCounter.hpp and befriended by
// both dictionaries; never defined in production. Reading the counter DIRECTLY is
// what lets "a read left the counter unmoved" and "a write bumped it EXACTLY
// once" be assertions rather than inferences from an enumerator's silence -- and
// "exactly once" is not observable through fail-fast alone.
//
// Spelled token-for-token as DictionaryEnumeratorKeyValueSafetyTests.cpp spells
// it, deliberately: these specialisations share a binary, so a divergent
// definition of the same explicit specialisation would be an ODR violation.
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

unsigned long long versionOf(const Hashtable& t) {
    return SharpRuntime::Testing::CollectionVersionAccess<Hashtable>::version(t);
}
unsigned long long versionOf(const ListDictionaryInternal& d) {
    return SharpRuntime::Testing::CollectionVersionAccess<ListDictionaryInternal>::version(d);
}

/**
 * @brief A value type with an observable lifetime, so an "owning copy" assertion
 *        is about a real object and not about a stale integer that happens to
 *        still read back correctly.
 */
struct Tracked {
    static int live;
    static int copies;
    std::string payload;

    explicit Tracked(std::string p = "") : payload(std::move(p)) { ++live; }
    Tracked(const Tracked& o) : payload(o.payload) { ++live; ++copies; }
    Tracked& operator=(const Tracked& o) { payload = o.payload; ++copies; return *this; }
    ~Tracked() { --live; }

    static void reset() { live = 0; copies = 0; }
};
int Tracked::live = 0;
int Tracked::copies = 0;

// ===========================================================================
// 1. IDictionary::getItem -- parameterised over BOTH implementations
//
// This is the member whose signature changed on the INTERFACE, so every
// assertion here must hold for every implementer or the interface is a lie.
// ===========================================================================

enum class Implementation { HashtableImpl, ListDictionaryInternalImpl };

/** @brief Uniform driver for either non-generic IDictionary's getItem contract. */
class GetItemHarness {
public:
    virtual ~GetItemHarness() = default;
    virtual IDictionary& dictionary() = 0;
    /** @brief Adds one entry and returns a raw key that names it. */
    virtual const void* addEntry() = 0;
    /** @brief A raw key that names no entry. */
    virtual const void* absentKey() = 0;
    /** @brief True when @p boxed holds the value the matching addEntry() stored. */
    virtual bool holdsTheStoredValue(const std::any& boxed) const = 0;
    virtual unsigned long long version() const = 0;
    virtual const char* name() const = 0;
};

class HashtableGetItemHarness : public GetItemHarness {
    Hashtable table_;
    std::vector<std::unique_ptr<int>> anchors_;
    std::vector<std::any> values_;
    int next_ = 0;
    int absent_ = -1;

public:
    IDictionary& dictionary() override { return table_; }
    const void* addEntry() override {
        anchors_.push_back(std::make_unique<int>(next_));
        values_.push_back(std::any(next_ * 10));
        table_.Add(static_cast<const void*>(anchors_.back().get()), &values_.back());
        ++next_;
        return anchors_.back().get();
    }
    const void* absentKey() override { return &absent_; }
    bool holdsTheStoredValue(const std::any& boxed) const override {
        return boxed.type() == typeid(int) && std::any_cast<int>(boxed) == (next_ - 1) * 10;
    }
    unsigned long long version() const override { return versionOf(table_); }
    const char* name() const override { return "Hashtable"; }
};

class ListDictionaryInternalGetItemHarness : public GetItemHarness {
    ListDictionaryInternal dict_;
    std::vector<std::unique_ptr<int>> keys_;
    std::vector<std::unique_ptr<int>> values_;
    int next_ = 0;
    int absent_ = -1;

public:
    IDictionary& dictionary() override { return dict_; }
    const void* addEntry() override {
        keys_.push_back(std::make_unique<int>(next_));
        values_.push_back(std::make_unique<int>(next_ * 10));
        dict_.Add(static_cast<const void*>(keys_.back().get()), values_.back().get());
        ++next_;
        return keys_.back().get();
    }
    const void* absentKey() override { return &absent_; }
    bool holdsTheStoredValue(const std::any& boxed) const override {
        // This implementation boxes the CALLER'S OWN pointer -- it stores no value
        // data of its own -- so the box holds void*, not the pointee's type.
        return boxed.type() == typeid(void*) &&
               std::any_cast<void*>(boxed) == values_.back().get();
    }
    unsigned long long version() const override { return versionOf(dict_); }
    const char* name() const override { return "ListDictionaryInternal"; }
};

class DictionaryGetItemContract : public ::testing::TestWithParam<Implementation> {
    Owning<GetItemHarness> harness_;

protected:
    void SetUp() override {
        if (GetParam() == Implementation::HashtableImpl)
            harness_ = std::make_unique<HashtableGetItemHarness>();
        else
            harness_ = std::make_unique<ListDictionaryInternalGetItemHarness>();
    }
    GetItemHarness& harness() { return *harness_; }
};

INSTANTIATE_TEST_SUITE_P(
    AllNonGenericDictionaries, DictionaryGetItemContract,
    ::testing::Values(Implementation::HashtableImpl,
                      Implementation::ListDictionaryInternalImpl),
    [](const ::testing::TestParamInfo<Implementation>& info) {
        return info.param == Implementation::HashtableImpl ? "Hashtable"
                                                           : "ListDictionaryInternal";
    });

TEST_P(DictionaryGetItemContract, GetItemReturnsAnOwningBoxForAPresentKey) {
    const void* key = harness().addEntry();
    const std::any boxed = harness().dictionary().getItem(key);
    ASSERT_TRUE(boxed.has_value());
    EXPECT_TRUE(harness().holdsTheStoredValue(boxed));
}

TEST_P(DictionaryGetItemContract, GetItemReturnsAnEmptyAnyForAnAbsentKey) {
    harness().addEntry();
    const std::any boxed = harness().dictionary().getItem(harness().absentKey());
    EXPECT_FALSE(boxed.has_value())
        << "an absent key must read as an empty std::any, matching .NET's getter returning null";
}

TEST_P(DictionaryGetItemContract, GetItemNeverInserts) {
    harness().addEntry();
    const intcs before = harness().dictionary().getCountProperty();
    (void)harness().dictionary().getItem(harness().absentKey());
    (void)harness().dictionary().getItem(harness().absentKey());
    EXPECT_EQ(harness().dictionary().getCountProperty(), before)
        << "reading a missing key must not structurally insert";
}

TEST_P(DictionaryGetItemContract, GetItemNeverAdvancesTheMutationCounter) {
    const void* key = harness().addEntry();
    const unsigned long long before = harness().version();
    (void)harness().dictionary().getItem(key);
    (void)harness().dictionary().getItem(harness().absentKey());
    EXPECT_EQ(harness().version(), before) << "the getter is pure, as .NET's is";
}

TEST_P(DictionaryGetItemContract, AnOutstandingEnumeratorSurvivesEveryGetItemRead) {
    harness().addEntry();
    harness().addEntry();
    Owning<IDictionaryEnumerator> e(harness().dictionary().GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    (void)harness().dictionary().getItem(harness().absentKey());

    EXPECT_NO_THROW((void)e->MoveNext())
        << "no read may invalidate an outstanding enumerator";
}

TEST_P(DictionaryGetItemContract, TheReturnedBoxIsIndependentOfTheDictionary) {
    const void* key = harness().addEntry();
    const std::any boxed = harness().dictionary().getItem(key);
    ASSERT_TRUE(boxed.has_value());

    harness().dictionary().Clear();

    // The box is the caller's own; clearing the dictionary cannot reach it.
    EXPECT_TRUE(harness().holdsTheStoredValue(boxed));
}

// ===========================================================================
// 2. Versioning -- every write bumps EXACTLY once, every read not at all
// ===========================================================================

TEST(HashtableValueAccessVersioning, ProxyAssignmentBumpsOnceOnInsert) {
    Hashtable t;
    const unsigned long long before = versionOf(t);
    t["alpha"] = std::any(1);
    EXPECT_EQ(versionOf(t), before + 1);
    EXPECT_EQ(t.getCountProperty(), 1);
}

TEST(HashtableValueAccessVersioning, ProxyAssignmentBumpsOnceOnReplace) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));
    const unsigned long long before = versionOf(t);
    t["alpha"] = std::any(2);
    EXPECT_EQ(versionOf(t), before + 1);
    EXPECT_EQ(t.getCountProperty(), 1);
    EXPECT_EQ(std::any_cast<int>(t.at("alpha")), 2);
}

TEST(HashtableValueAccessVersioning, AnEqualValueReplacementStillBumps) {
    // .NET's Hashtable.Insert calls UpdateVersion() on the overwrite branch and
    // NEVER compares the old value, so an equal-value write invalidates
    // enumerators. This is deliberately the OPPOSITE of Generic::Dictionary's
    // rule in this same component, and both match their own .NET reference.
    Hashtable t;
    t.Add(std::string("alpha"), std::any(7));
    const unsigned long long before = versionOf(t);
    t["alpha"] = std::any(7);
    EXPECT_EQ(versionOf(t), before + 1);
}

TEST(HashtableValueAccessVersioning, TypedSetItemBumpsOnceOnInsertAndOnReplace) {
    Hashtable t;
    const unsigned long long start = versionOf(t);
    t.setItem(std::string("alpha"), std::any(1));
    EXPECT_EQ(versionOf(t), start + 1);
    t.setItem(std::string("alpha"), std::any(2));
    EXPECT_EQ(versionOf(t), start + 2);
    t.setItem(std::string("alpha"), std::any(2)); // equal replace
    EXPECT_EQ(versionOf(t), start + 3);
    EXPECT_EQ(t.getCountProperty(), 1);
}

TEST(HashtableValueAccessVersioning, RawKeySetItemBumpsOnceOnInsertAndOnReplace) {
    Hashtable t;
    int anchor = 1;
    std::any first = std::any(1);
    std::any second = std::any(2);
    const unsigned long long start = versionOf(t);
    t.setItem(&anchor, &first);
    EXPECT_EQ(versionOf(t), start + 1);
    t.setItem(&anchor, &second);
    EXPECT_EQ(versionOf(t), start + 2);
    EXPECT_EQ(t.getCountProperty(), 1);
}

TEST(HashtableValueAccessVersioning, BothAddOverloadsBumpOnceAndRejectADuplicate) {
    Hashtable t;
    int anchor = 1;
    std::any value = std::any(1);

    const unsigned long long start = versionOf(t);
    t.Add(std::string("alpha"), std::any(1));
    EXPECT_EQ(versionOf(t), start + 1);
    t.Add(static_cast<const void*>(&anchor), &value);
    EXPECT_EQ(versionOf(t), start + 2);

    // Add rejects an existing key where the indexer setter replaces it -- the
    // .NET distinction, preserved. A rejected Add must not move the counter.
    const unsigned long long beforeDuplicate = versionOf(t);
    EXPECT_THROW(t.Add(std::string("alpha"), std::any(9)), System::ArgumentException);
    EXPECT_THROW(t.Add(static_cast<const void*>(&anchor), &value), System::ArgumentException);
    EXPECT_EQ(versionOf(t), beforeDuplicate) << "a throwing write advances nothing";
    EXPECT_EQ(t.getCountProperty(), 2);
    EXPECT_EQ(std::any_cast<int>(t.at("alpha")), 1) << "and changes nothing";
}

TEST(HashtableValueAccessVersioning, NoReadPathAdvancesTheCounter) {
    Hashtable t;
    int anchor = 1;
    std::any value = std::any(1);
    t.Add(static_cast<const void*>(&anchor), &value);
    t.Add(std::string("alpha"), std::any(2));
    const Hashtable& ct = t;

    const unsigned long long before = versionOf(t);

    (void)std::any(t["alpha"]);            // proxy read conversion, present
    (void)std::any(t["missing"]);          // proxy read conversion, absent
    (void)t["alpha"].getValueProperty();   // named proxy read
    (void)t["missing"].hasValueProperty(); // proxy presence test
    (void)ct["alpha"];                     // const operator[]
    (void)ct["missing"];                   // const operator[], absent
    (void)t.at("alpha");                   // throwing read, success
    EXPECT_THROW((void)t.at("missing"), KeyNotFoundException); // and failure
    (void)t.getItem(&anchor);              // raw-key read
    (void)t.Contains(&anchor);
    (void)t.ContainsKey("alpha");
    (void)t.ContainsValue(std::any(2));
    (void)t.getKeys();
    (void)t.getValues();

    EXPECT_EQ(versionOf(t), before)
        << "every read accessor is pure; only writes move the counter";
}

// ===========================================================================
// 3. A read NEVER inserts -- the defect with no sanitizer report
// ===========================================================================

TEST(HashtableValueAccessRead, ReadingAMissingKeyLeavesTheTableStructurallyUntouched) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));
    const Hashtable& ct = t;

    const intcs count = t.getCountProperty();
    const unsigned long long version = versionOf(t);
    const std::vector<std::string> keys = t.getKeys();
    const std::vector<std::any> values = t.getValues();

    (void)std::any(t["ghost"]);
    (void)t["ghost"].getValueProperty();
    (void)ct["ghost"];
    (void)t.getItem(&t);

    EXPECT_EQ(t.getCountProperty(), count);
    EXPECT_EQ(versionOf(t), version);
    EXPECT_FALSE(t.ContainsKey("ghost"))
        << "before #1796 a bare read inserted a default-constructed entry here";
    EXPECT_EQ(t.getKeys().size(), keys.size());
    EXPECT_EQ(t.getValues().size(), values.size());

    // And the key/value views agree.
    Owning<ICollection> keyView(t.getKeysProperty());
    Owning<ICollection> valueView(t.getValuesProperty());
    EXPECT_EQ(keyView->getCountProperty(), count);
    EXPECT_EQ(valueView->getCountProperty(), count);
}

TEST(HashtableValueAccessRead, MissingKeyReadsAcrossALargeTableDoNotRehashOrCorruptEnumeration) {
    // The pre-#1796 reproduction: 4,008 entries, an untracked structural insert
    // through operator[], an outstanding enumerator that walked 2,045 distinct
    // keys, reached 6 of its 8 seed keys and threw nothing. Post-fix the read
    // inserts nothing, so the walk is complete and exact.
    Hashtable t;
    for (int i = 0; i < 4000; ++i) t.Add("k" + std::to_string(i), std::any(i));
    for (int i = 0; i < 8; ++i) t.Add("seed" + std::to_string(i), std::any(i));
    ASSERT_EQ(t.getCountProperty(), 4008);

    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    for (int i = 0; i < 64; ++i) (void)std::any(t["absent" + std::to_string(i)]);
    ASSERT_EQ(t.getCountProperty(), 4008) << "not one read inserted";

    std::set<std::string> seen;
    seen.insert(std::any_cast<std::string>(e->getKeyProperty()));
    while (e->MoveNext()) seen.insert(std::any_cast<std::string>(e->getKeyProperty()));

    EXPECT_EQ(seen.size(), 4008u) << "the walk is complete";
    for (int i = 0; i < 8; ++i)
        EXPECT_TRUE(seen.count("seed" + std::to_string(i)) != 0)
            << "every seed key was reached";
}

TEST(HashtableValueAccessRead, AProxyForAMissingKeyDoesNotInsertUntilItIsAssigned) {
    Hashtable t;
    {
        Hashtable::ValueReference r = t["deferred"];
        EXPECT_FALSE(r.hasValueProperty());
        EXPECT_FALSE(r.getValueProperty().has_value());
        EXPECT_EQ(t.getCountProperty(), 0) << "holding a proxy inserts nothing";
        r = std::any(5);
        EXPECT_EQ(t.getCountProperty(), 1) << "assignment is what inserts";
        EXPECT_TRUE(r.hasValueProperty());
    }
    EXPECT_EQ(t.getCountProperty(), 1) << "and destroying the proxy changes nothing";
    EXPECT_EQ(std::any_cast<int>(t.at("deferred")), 5);
}

// ===========================================================================
// 4. Fail-fast -- a tracked write DOES invalidate outstanding enumerators
// ===========================================================================

TEST(HashtableValueAccessFailFast, AnEnumeratorFailsFastAfterAnIndexerInsert) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));
    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    t["beta"] = std::any(2);

    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
    EXPECT_THROW(e->Reset(), System::InvalidOperationException);
}

TEST(HashtableValueAccessFailFast, AnEnumeratorFailsFastAfterAnIndexerReplace) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));
    t.Add(std::string("beta"), std::any(2));
    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    t["alpha"] = std::any(99);

    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException)
        << "before #1796 this walked to the end without throwing";
}

TEST(HashtableValueAccessFailFast, AnEnumeratorFailsFastAfterAnEqualValueReplace) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(7));
    t.Add(std::string("beta"), std::any(8));
    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    t["alpha"] = std::any(7);

    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
}

TEST(HashtableValueAccessFailFast, ALiveValueViewFailsFastAfterAnIndexerWrite) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));
    t.Add(std::string("beta"), std::any(2));

    Owning<ICollection> valueView(t.getValuesProperty());
    Owning<IEnumerator> ve(valueView->GetEnumerator());
    ASSERT_TRUE(ve->MoveNext());

    Owning<ICollection> keyView(t.getKeysProperty());
    Owning<IEnumerator> ke(keyView->GetEnumerator());
    ASSERT_TRUE(ke->MoveNext());

    t["alpha"] = std::any(42);

    EXPECT_THROW((void)ve->MoveNext(), System::InvalidOperationException)
        << "before #1796 a live value view walked past an untracked write";
    EXPECT_THROW((void)ke->MoveNext(), System::InvalidOperationException);
}

TEST(HashtableValueAccessFailFast, EveryReadLeavesEveryOutstandingViewValid) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));
    t.Add(std::string("beta"), std::any(2));
    const Hashtable& ct = t;

    Owning<ICollection> valueView(t.getValuesProperty());
    Owning<IEnumerator> ve(valueView->GetEnumerator());
    ASSERT_TRUE(ve->MoveNext());

    (void)std::any(t["alpha"]);
    (void)std::any(t["missing"]);
    (void)ct["alpha"];
    (void)t.at("alpha");
    (void)t.getItem(&t);

    EXPECT_NO_THROW((void)ve->MoveNext());
}

// ===========================================================================
// 5. Ownership and lifetime -- the nine heap-use-after-free reports
//
// Every scenario below produced an AddressSanitizer heap-use-after-free before
// #1796 through a retained std::any& / void* / const std::any&. A retained COPY
// is inexpressible as an alias, so these are now ordinary value assertions.
// ===========================================================================

TEST(HashtableValueAccessLifetime, AReadValueSurvivesRehash) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(std::string("kept")));
    const std::any kept = t["alpha"];

    for (int i = 0; i < 8000; ++i) t.Add("filler" + std::to_string(i), std::any(i));

    EXPECT_EQ(std::any_cast<std::string>(kept), "kept");
}

TEST(HashtableValueAccessLifetime, AReadValueSurvivesRemoveClearAssignmentAndDestruction) {
    Tracked::reset();
    std::any afterRemove, afterClear, afterCopyAssign, afterMoveAssign, afterDestroy;
    {
        Hashtable t;
        t.Add(std::string("alpha"), std::any(Tracked("alpha-payload")));

        afterRemove = t["alpha"];
        t.Remove(std::string("alpha"));
        EXPECT_EQ(std::any_cast<Tracked>(afterRemove).payload, "alpha-payload");

        t.setItem(std::string("alpha"), std::any(Tracked("second")));
        afterClear = t["alpha"];
        t.Clear();
        EXPECT_EQ(std::any_cast<Tracked>(afterClear).payload, "second");

        t.setItem(std::string("alpha"), std::any(Tracked("third")));
        afterCopyAssign = t.at("alpha");
        Hashtable other;
        other.Add(std::string("z"), std::any(Tracked("other")));
        t = other;                                   // copy assignment
        EXPECT_EQ(std::any_cast<Tracked>(afterCopyAssign).payload, "third");

        t.setItem(std::string("alpha"), std::any(Tracked("fourth")));
        afterMoveAssign = t.at("alpha");
        Hashtable third;
        third.Add(std::string("y"), std::any(Tracked("third-table")));
        t = std::move(third);                        // move assignment
        EXPECT_EQ(std::any_cast<Tracked>(afterMoveAssign).payload, "fourth");

        Hashtable doomed;
        doomed.Add(std::string("d"), std::any(Tracked("doomed")));
        afterDestroy = doomed["d"];
    }                                                 // every table destroyed here
    EXPECT_EQ(std::any_cast<Tracked>(afterDestroy).payload, "doomed");

    afterRemove.reset(); afterClear.reset(); afterCopyAssign.reset();
    afterMoveAssign.reset(); afterDestroy.reset();
    EXPECT_EQ(Tracked::live, 0) << "and nothing leaked";
}

TEST(HashtableValueAccessLifetime, AProxyIsValidForTheFullExpressionThatMadeIt) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));

    // The temporary proxy is destroyed at the end of each full-expression; the
    // value it produced is the caller's own and outlives it.
    const std::any first = t["alpha"];
    t["alpha"] = std::any(2);
    const std::any second = t["alpha"];

    EXPECT_EQ(std::any_cast<int>(first), 1) << "the first read is a snapshot, not a view";
    EXPECT_EQ(std::any_cast<int>(second), 2);
}

TEST(HashtableValueAccessLifetime, AProxySurvivesRehashAndFollowsItsKeyNotASlot) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));

    Hashtable::ValueReference r = t["alpha"];
    for (int i = 0; i < 8000; ++i) t.Add("filler" + std::to_string(i), std::any(i));

    // The proxy holds a KEY, not a slot pointer, so rehashing is irrelevant to it.
    EXPECT_EQ(std::any_cast<int>(r.getValueProperty()), 1);
    r = std::any(2);
    EXPECT_EQ(std::any_cast<int>(t.at("alpha")), 2);
}

TEST(HashtableValueAccessLifetime, AProxyRemainsUsableAcrossRemoveAndReinsertOfItsKey) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));

    Hashtable::ValueReference r = t["alpha"];
    t.Remove(std::string("alpha"));

    // Erasing the entry does not invalidate the proxy: it names a key, and the key
    // is simply absent now. Reading it is defined and yields empty.
    EXPECT_FALSE(r.hasValueProperty());
    EXPECT_FALSE(r.getValueProperty().has_value());

    r = std::any(3);                     // and assigning re-inserts
    EXPECT_EQ(t.getCountProperty(), 1);
    EXPECT_EQ(std::any_cast<int>(t.at("alpha")), 3);
}

TEST(HashtableValueAccessLifetime, ReadingCopiesTheValueRatherThanSharingIt) {
    Tracked::reset();
    Hashtable t;
    t.Add(std::string("alpha"), std::any(Tracked("original")));

    std::any copy = t["alpha"];
    std::any_cast<Tracked&>(copy).payload = "mutated-copy";

    EXPECT_EQ(std::any_cast<Tracked>(t.at("alpha")).payload, "original")
        << "mutating the returned copy cannot reach the table";
}

// ===========================================================================
// 6. Exceptions
// ===========================================================================

TEST(HashtableValueAccessExceptions, AtThrowsKeyNotFoundForAnAbsentKey) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));
    EXPECT_THROW((void)t.at("missing"), KeyNotFoundException);
}

TEST(HashtableValueAccessExceptions, AtsExceptionIsCatchableAsASystemException) {
    // The point of the change: std::out_of_range is invisible to a consumer that
    // catches the port's own exception root, which is what ported C# code does.
    Hashtable t;
    bool caught = false;
    std::string message;
    intcs hresult = 0;
    try {
        (void)t.at("missing");
    } catch (const System::Exception& e) {
        caught = true;
        message = e.getMessageProperty();
        hresult = e.getHResultProperty();
    }
    EXPECT_TRUE(caught) << "catch (const System::Exception&) must see it";
    EXPECT_EQ(message, "The given key 'missing' was not present in the dictionary.");
    EXPECT_EQ(hresult, System::Collections::Generic::KeyNotFoundException().getHResultProperty());
}

TEST(HashtableValueAccessExceptions, AtsMessageNamesTheKeyAndIsStable) {
    Hashtable t;
    for (const std::string& key : {std::string(""), std::string("0"), std::string("odd key")}) {
        try {
            (void)t.at(key);
            ADD_FAILURE() << "at(\"" << key << "\") should have thrown";
        } catch (const KeyNotFoundException& e) {
            EXPECT_EQ(e.getMessageProperty(),
                      "The given key '" + key + "' was not present in the dictionary.");
        }
    }
}

TEST(HashtableValueAccessExceptions, AFailedAtLeavesTheTableAndItsEnumeratorsUntouched) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));
    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    const unsigned long long before = versionOf(t);
    EXPECT_THROW((void)t.at("missing"), KeyNotFoundException);

    EXPECT_EQ(versionOf(t), before);
    EXPECT_EQ(t.getCountProperty(), 1);
    EXPECT_NO_THROW((void)e->MoveNext());
}

TEST(HashtableValueAccessExceptions, EveryHashtableRawKeyEntryPointRejectsANullKey) {
    Hashtable t;
    std::any value = std::any(1);
    EXPECT_THROW((void)t.getItem(nullptr), System::ArgumentNullException);
    EXPECT_THROW(t.setItem(nullptr, &value), System::ArgumentNullException);
    EXPECT_THROW(t.Add(nullptr, &value), System::ArgumentNullException);
    EXPECT_THROW((void)t.Contains(nullptr), System::ArgumentNullException);
    EXPECT_THROW(t.Remove(static_cast<const void*>(nullptr)), System::ArgumentNullException);
    EXPECT_THROW(t.Remove(static_cast<const char*>(nullptr)), System::ArgumentNullException);
    EXPECT_EQ(t.getCountProperty(), 0) << "and none of them inserted anything";
}

TEST(HashtableValueAccessExceptions, ListDictionaryInternalStillAcceptsANullKeyAndThatIsTicket1798) {
    // NOT a #1796 assertion and NOT fixed here. Pinned so that the divergence
    // between the two IDictionary implementations is a recorded, deliberate state
    // rather than an unnoticed one, and so #1798 has a test to flip.
    ListDictionaryInternal d;
    int value = 1;
    EXPECT_NO_THROW((void)d.getItem(nullptr));
    EXPECT_NO_THROW(d.setItem(nullptr, &value));
    EXPECT_EQ(d.getCountProperty(), 1)
        << "ListDictionaryInternal stores a null key where .NET and Hashtable throw (#1798)";
}

// ===========================================================================
// 7. std::any semantics -- payload types, nesting, and wrong casts
// ===========================================================================

TEST(HashtableValueAccessTypes, EveryReadPathBoxesTheStoredPayloadNotANestedAny) {
    Hashtable t;
    int anchor = 1;
    std::any stored = std::any(std::string("payload"));
    t.setItem(static_cast<const void*>(&anchor), &stored);
    t.setItem(std::string("alpha"), std::any(std::string("payload")));
    const Hashtable& ct = t;

    EXPECT_EQ(t.at("alpha").type(), typeid(std::string));
    EXPECT_EQ(std::any(t["alpha"]).type(), typeid(std::string));
    EXPECT_EQ(t["alpha"].getValueProperty().type(), typeid(std::string));
    EXPECT_EQ(ct["alpha"].type(), typeid(std::string));
    EXPECT_EQ(t.getItem(&anchor).type(), typeid(std::string));

    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    EXPECT_EQ(e->getValueProperty().type(), typeid(std::string));
}

TEST(HashtableValueAccessTypes, ADeliberatelyNestedAnyRoundTripsWithoutFlattening) {
    Hashtable t;
    t.setItem(std::string("nested"), std::make_any<std::any>(std::any(5)));

    const std::any outer = t.at("nested");
    ASSERT_EQ(outer.type(), typeid(std::any));
    const std::any inner = std::any_cast<std::any>(outer);
    EXPECT_EQ(inner.type(), typeid(int));
    EXPECT_EQ(std::any_cast<int>(inner), 5);

    // And an ordinary std::any argument is a COPY, so it stores int, not any.
    t.setItem(std::string("plain"), std::any(std::any(5)));
    EXPECT_EQ(t.at("plain").type(), typeid(int));
}

TEST(HashtableValueAccessTypes, ReadingAProxyIntoAnAnyProducesTheValueNotABoxedProxy) {
    // The run-time trap the non-copyable proxy exists to prevent: with a COPYABLE
    // proxy, std::any's template converting constructor outranks the proxy's own
    // conversion operator and `v` would silently hold a ValueReference.
    Hashtable t;
    t.Add(std::string("alpha"), std::any(41));

    std::any v = t["alpha"];
    ASSERT_EQ(v.type(), typeid(int)) << "must be the value, never the proxy";
    EXPECT_EQ(std::any_cast<int>(v), 41);

    auto deduced = t["alpha"].getValueProperty();
    static_assert(std::is_same_v<decltype(deduced), std::any>);
    EXPECT_EQ(std::any_cast<int>(deduced), 41);

    static_assert(!std::is_copy_constructible_v<Hashtable::ValueReference>,
                  "the proxy must not be copy-constructible");
    static_assert(!std::is_copy_assignable_v<Hashtable::ValueReference>,
                  "the proxy must not be copy-assignable");
    static_assert(!std::is_move_constructible_v<Hashtable::ValueReference>,
                  "the proxy must not be move-constructible either");
    static_assert(std::is_same_v<decltype(std::declval<Hashtable&>()["k"]),
                                 Hashtable::ValueReference>);
    static_assert(std::is_same_v<decltype(std::declval<const Hashtable&>()["k"]), std::any>);
    static_assert(std::is_same_v<decltype(std::declval<const Hashtable&>().at("k")), std::any>);
    static_assert(std::is_same_v<
        decltype(std::declval<const Hashtable&>().getItem(nullptr)), std::any>);
}

TEST(HashtableValueAccessTypes, AWrongAnyCastThrowsRatherThanReinterpreting) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(std::string("text")));
    EXPECT_THROW((void)std::any_cast<int>(t.at("alpha")), std::bad_any_cast);
    EXPECT_THROW((void)std::any_cast<int>(std::any(t["alpha"])), std::bad_any_cast);
    EXPECT_NO_THROW((void)std::any_cast<std::string>(t.at("alpha")));
}

TEST(HashtableValueAccessTypes, TheCommonPayloadTypesAllRoundTrip) {
    Tracked::reset();
    {
        Hashtable t;
        auto shared = std::make_shared<int>(7);
        int target = 3;

        t.setItem(std::string("int"), std::any(42));
        t.setItem(std::string("string"), std::any(std::string("a string longer than the SSO buffer can hold")));
        t.setItem(std::string("entry"), std::any(DictionaryEntry(std::string("k"), std::any(1))));
        t.setItem(std::string("pointer"), std::any(&target));
        t.setItem(std::string("shared"), std::any(shared));
        t.setItem(std::string("tracked"), std::any(Tracked("owned")));

        EXPECT_EQ(std::any_cast<int>(t.at("int")), 42);
        EXPECT_EQ(std::any_cast<std::string>(t.at("string")).substr(0, 8), "a string");
        EXPECT_EQ(std::any_cast<int>(
            std::any_cast<DictionaryEntry>(t.at("entry")).getValueProperty()), 1);
        EXPECT_EQ(std::any_cast<int*>(t.at("pointer")), &target);
        EXPECT_EQ(*std::any_cast<std::shared_ptr<int>>(t.at("shared")), 7);
        EXPECT_EQ(std::any_cast<Tracked>(t.at("tracked")).payload, "owned");

        // Reading a shared_ptr value shares ownership; it does not clone the pointee.
        EXPECT_EQ(std::any_cast<std::shared_ptr<int>>(t.at("shared")).get(), shared.get());
    }
    EXPECT_EQ(Tracked::live, 0);
}

// ===========================================================================
// 8. Pointer-valued entries -- two levels that must not be conflated
// ===========================================================================

TEST(HashtableValueAccessPointers, MutatingThePointeeIsNotADictionaryMutation) {
    Hashtable t;
    int target = 1;
    t.setItem(std::string("ptr"), std::any(&target));

    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    const unsigned long long before = versionOf(t);

    *std::any_cast<int*>(t.at("ptr")) = 100;

    EXPECT_EQ(target, 100) << "the pointee changed, as it must";
    EXPECT_EQ(versionOf(t), before) << "but the DICTIONARY did not";
    EXPECT_NO_THROW((void)e->MoveNext()) << "so an enumerator stays valid";
}

TEST(HashtableValueAccessPointers, ReplacingTheStoredPointerIsADictionaryMutation) {
    Hashtable t;
    int first = 1, second = 2;
    t.setItem(std::string("ptr"), std::any(&first));

    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    const unsigned long long before = versionOf(t);

    t["ptr"] = std::any(&second);

    EXPECT_EQ(versionOf(t), before + 1);
    EXPECT_EQ(std::any_cast<int*>(t.at("ptr")), &second);
    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
}

TEST(HashtableValueAccessPointers, ReplacingTheRETURNEDPointerDoesNotReplaceTheStoredOne) {
    Hashtable t;
    int first = 1, second = 2;
    t.setItem(std::string("ptr"), std::any(&first));

    std::any read = t.at("ptr");
    std::any_cast<int*&>(read) = &second;   // rebinding the COPY

    EXPECT_EQ(std::any_cast<int*>(t.at("ptr")), &first)
        << "the read is a copy of the pointer; rebinding it cannot reach the table";
}

// ===========================================================================
// 9. The proxy's own contract
// ===========================================================================

TEST(HashtableValueAccessProxy, HasValuePropertyDistinguishesAbsentFromEmpty) {
    Hashtable t;
    t.setItem(std::string("empty"), std::any{});

    EXPECT_TRUE(t["empty"].hasValueProperty()) << "present, holding an empty std::any";
    EXPECT_FALSE(t["empty"].getValueProperty().has_value());
    EXPECT_FALSE(t["absent"].hasValueProperty());
    EXPECT_FALSE(t["absent"].getValueProperty().has_value());

    // Which is exactly why the reads alone cannot tell them apart -- .NET's getter
    // returns null for both, and ContainsKey is the discriminator.
    EXPECT_TRUE(t.ContainsKey("empty"));
    EXPECT_FALSE(t.ContainsKey("absent"));
    EXPECT_FALSE(t.at("empty").has_value());
    EXPECT_THROW((void)t.at("absent"), KeyNotFoundException);
}

TEST(HashtableValueAccessProxy, ProxyToProxyAssignmentWritesThroughAndBumps) {
    Hashtable t;
    t.Add(std::string("source"), std::any(11));
    t.Add(std::string("target"), std::any(22));
    const unsigned long long before = versionOf(t);

    t["target"] = t["source"];

    EXPECT_EQ(std::any_cast<int>(t.at("target")), 11);
    EXPECT_EQ(std::any_cast<int>(t.at("source")), 11) << "the source is unchanged";
    EXPECT_EQ(versionOf(t), before + 1) << "one tracked write";
    EXPECT_EQ(t.getCountProperty(), 2);
}

TEST(HashtableValueAccessProxy, ProxyAssignmentAcceptsEveryOrdinaryValueSpelling) {
    Hashtable t;
    t["int"] = std::any(1);
    t["string"] = std::any(std::string("s"));
    t["empty"] = std::any{};
    EXPECT_EQ(t.getCountProperty(), 3);
    EXPECT_EQ(std::any_cast<int>(t.at("int")), 1);
    EXPECT_EQ(std::any_cast<std::string>(t.at("string")), "s");
    EXPECT_FALSE(t.at("empty").has_value());
}

TEST(HashtableValueAccessProxy, TheConstOverloadIsTheOneChosenForAConstTable) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));
    const Hashtable& ct = t;

    const std::any read = ct["alpha"];
    EXPECT_EQ(std::any_cast<int>(read), 1);
    EXPECT_FALSE(ct["missing"].has_value());
    EXPECT_EQ(t.getCountProperty(), 1) << "a const read cannot insert";
}

// ===========================================================================
// 10. Key-shape matrix -- every key form this port supports
// ===========================================================================

TEST(HashtableValueAccessKeys, EveryStringKeyShapeBehavesIdenticallyThroughEveryAccessor) {
    for (const std::string& key : {std::string(""), std::string("0"),
                                   std::string("ordinary"), std::string("94525064757436")}) {
        Hashtable t;
        const Hashtable& ct = t;

        // Absent.
        EXPECT_FALSE(t.ContainsKey(key)) << "key=" << key;
        EXPECT_FALSE(std::any(t[key]).has_value()) << "key=" << key;
        EXPECT_FALSE(ct[key].has_value()) << "key=" << key;
        EXPECT_THROW((void)t.at(key), KeyNotFoundException) << "key=" << key;
        EXPECT_EQ(t.getCountProperty(), 0) << "key=" << key;

        // Inserted through the proxy.
        t[key] = std::any(7);
        EXPECT_TRUE(t.ContainsKey(key)) << "key=" << key;
        EXPECT_EQ(std::any_cast<int>(t.at(key)), 7) << "key=" << key;
        EXPECT_EQ(std::any_cast<int>(ct[key]), 7) << "key=" << key;
        EXPECT_EQ(t.getCountProperty(), 1) << "key=" << key;

        // Replaced, then removed.
        t.setItem(key, std::any(8));
        EXPECT_EQ(std::any_cast<int>(t.at(key)), 8) << "key=" << key;
        t.Remove(key);
        EXPECT_FALSE(t.ContainsKey(key)) << "key=" << key;
        EXPECT_EQ(t.getCountProperty(), 0) << "key=" << key;
    }
}

TEST(HashtableValueAccessKeys, ARawPointerKeyAndTheStringKeyZeroStillNameDifferentEntries) {
    // Ticket #1775's fix, re-asserted through the NEW accessors: a null raw key no
    // longer stringifies to "0", so the two key spaces cannot collide.
    Hashtable t;
    int anchor = 1;
    std::any value = std::any(10);

    t.Add(std::string("0"), std::any(7));
    t.Add(static_cast<const void*>(&anchor), &value);

    EXPECT_EQ(t.getCountProperty(), 2);
    EXPECT_EQ(std::any_cast<int>(t.at("0")), 7);
    EXPECT_EQ(std::any_cast<int>(t.getItem(&anchor)), 10);
    EXPECT_THROW((void)t.getItem(nullptr), System::ArgumentNullException);
    EXPECT_EQ(t.getCountProperty(), 2) << "and the rejected read inserted nothing";
}

TEST(HashtableValueAccessKeys, ARawKeyEntryIsReachableThroughTheStringIndexerAtItsAddressText) {
    // The raw-key surface stringifies the address; the string indexer sees the same
    // key space. Pinned so the two halves of the API cannot silently diverge.
    Hashtable t;
    int anchor = 1;
    std::any value = std::any(5);
    t.Add(static_cast<const void*>(&anchor), &value);

    const std::string addressText =
        std::to_string(reinterpret_cast<uintptr_t>(static_cast<const void*>(&anchor)));
    EXPECT_TRUE(t.ContainsKey(addressText));
    EXPECT_EQ(std::any_cast<int>(t.at(addressText)), 5);

    const unsigned long long before = versionOf(t);
    t[addressText] = std::any(6);
    EXPECT_EQ(versionOf(t), before + 1);
    EXPECT_EQ(std::any_cast<int>(t.getItem(&anchor)), 6);
}

TEST(HashtableValueAccessKeys, AddAndTheIndexerSetterKeepTheirDistinctMeanings) {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));

    EXPECT_THROW(t.Add(std::string("alpha"), std::any(2)), System::ArgumentException)
        << "Add rejects a duplicate";
    EXPECT_NO_THROW(t["alpha"] = std::any(2));
    EXPECT_EQ(std::any_cast<int>(t.at("alpha")), 2) << "the indexer setter replaces";
    EXPECT_NO_THROW(t.setItem(std::string("alpha"), std::any(3)));
    EXPECT_EQ(std::any_cast<int>(t.at("alpha")), 3) << "and so does setItem";
    EXPECT_EQ(t.getCountProperty(), 1);
}

} // namespace
