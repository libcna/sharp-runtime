// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1791
// (REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT), implementing the architecture
// selected by design ticket #1790, docs/ListIndexerVersioningDesign.md.
//
// #1790's own suite, ListIndexerVersionTests.cpp, pins the CONTRACT and records
// the flipped divergence. This file covers what #1791 added on top:
//
//   ListIndexerBasics        -- getItem/setItem and the tracked indexer across
//                               index positions, list shapes and element types.
//   ListIndexerVersioning    -- exactly which operations advance the counter,
//                               observed directly through the #1800 test seam,
//                               and which enumerators that invalidates.
//   ListIndexerProxyLanguage -- the C++ behaviour of the proxy itself: binding,
//                               conversion, compound assignment, comparison,
//                               copies, and the shapes that must NOT compile.
//   ListIndexerToVector      -- the closed structural escape.
//   ListIndexerImplementers  -- all FOUR IList<T> implementers, not just List<T>.
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/IList.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/ObjectModel/Collection.hpp"
#include "System/Collections/ObjectModel/ReadOnlyCollection.hpp"
#include "System/Collections/detail/ElementReference.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/NotSupportedException.hpp"
// The ONE authoritative definition of the mutation-counter test seam (ticket #1800).
// Never write a CollectionVersionAccess specialisation in a test translation unit.
#include "../../../support/CollectionVersionSeam.hpp"

using System::Collections::Generic::IEnumerator;
using System::Collections::Generic::IList;
using System::Collections::Generic::List;
using System::Collections::ObjectModel::Collection;
using System::Collections::ObjectModel::ReadOnlyCollection;
using System::Collections::detail::ElementReference;

namespace {

/** Owns the caller-owned enumerator GetEnumerator() hands back. */
template <typename T>
class ScopedEnumerator {
    IEnumerator<T>* e_;
public:
    explicit ScopedEnumerator(IList<T>& list) : e_(list.GetEnumerator()) {}
    ScopedEnumerator(const ScopedEnumerator&) = delete;
    ScopedEnumerator& operator=(const ScopedEnumerator&) = delete;
    ~ScopedEnumerator() { delete e_; }
    IEnumerator<T>* operator->() const { return e_; }
};

/** Reads a collection's private mutation counter through the one authoritative seam. */
template <typename TCollection>
auto versionOf(const TCollection& c)
{
    return SharpRuntime::Testing::CollectionVersionAccess<TCollection>::version(c);
}

/**
 * A non-trivial element type that counts its own operations. Constructions and assignments
 * are counted separately on purpose: only a construction creates an object that must later
 * be destroyed, so `constructed + copyConstructed + moveConstructed == destroyed` is the
 * balance an ownership leak would break, whereas an assignment creates nothing.
 */
struct Counted {
    static int constructed;
    static int copyConstructed;
    static int moveConstructed;
    static int copyAssigned;
    static int moveAssigned;
    static int destroyed;
    static void reset() {
        constructed = copyConstructed = moveConstructed = 0;
        copyAssigned = moveAssigned = destroyed = 0;
    }
    /** Every object that was created, by any constructor. */
    static int created() { return constructed + copyConstructed + moveConstructed; }

    int value = 0;
    std::string tag;

    Counted() { ++constructed; }
    explicit Counted(int v, std::string t = {}) : value(v), tag(std::move(t)) { ++constructed; }
    Counted(const Counted& o) : value(o.value), tag(o.tag) { ++copyConstructed; }
    Counted(Counted&& o) noexcept : value(o.value), tag(std::move(o.tag)) { ++moveConstructed; }
    Counted& operator=(const Counted& o) {
        value = o.value; tag = o.tag; ++copyAssigned; return *this;
    }
    Counted& operator=(Counted&& o) noexcept {
        value = o.value; tag = std::move(o.tag); ++moveAssigned; return *this;
    }
    ~Counted() { ++destroyed; }
    bool operator==(const Counted& o) const { return value == o.value && tag == o.tag; }
};
int Counted::constructed = 0;
int Counted::copyConstructed = 0;
int Counted::moveConstructed = 0;
int Counted::copyAssigned = 0;
int Counted::moveAssigned = 0;
int Counted::destroyed = 0;

/** An element type whose assignment throws, to pin the failed-write contract. */
struct ThrowsOnAssign {
    int value = 0;
    static bool armed;
    ThrowsOnAssign() = default;
    explicit ThrowsOnAssign(int v) : value(v) {}
    ThrowsOnAssign(const ThrowsOnAssign&) = default;
    ThrowsOnAssign& operator=(const ThrowsOnAssign& o) {
        if (armed) throw System::InvalidOperationException("assignment refused");
        value = o.value;
        return *this;
    }
    bool operator==(const ThrowsOnAssign& o) const { return value == o.value; }
};
bool ThrowsOnAssign::armed = false;

/** The fourth IList<T> implementer: hand-written, in ordinary consumer style. */
class HandWrittenList : public IList<int> {
    std::vector<int> data_;
    System::Collections::detail::MutationCounter version_;
public:
    IEnumerator<int>* GetEnumerator() override { return nullptr; }
    SharpRuntime::intcs getCountProperty() const override {
        return static_cast<SharpRuntime::intcs>(data_.size());
    }
    bool getIsReadOnlyProperty() const override { return false; }
    void Add(const int& item) override { data_.push_back(item); ++version_; }
    void Clear() override { data_.clear(); ++version_; }
    bool Contains(const int& item) const override {
        for (int v : data_) if (v == item) return true;
        return false;
    }
    bool Remove(const int&) override { return false; }
    const int& operator[](SharpRuntime::intcs i) const override {
        return data_[static_cast<std::size_t>(i)];
    }
    ElementReference<int> operator[](SharpRuntime::intcs i) override {
        return {&data_[static_cast<std::size_t>(i)], &version_};
    }
    const int& getItem(SharpRuntime::intcs i) const override {
        return data_[static_cast<std::size_t>(i)];
    }
    void setItem(SharpRuntime::intcs i, const int& value) override {
        ElementReference<int>{&data_[static_cast<std::size_t>(i)], &version_} = value;
    }
    SharpRuntime::intcs IndexOf(const int&) const override { return -1; }
    void Insert(SharpRuntime::intcs i, const int& item) override {
        data_.insert(data_.begin() + i, item);
        ++version_;
    }
    void RemoveAt(SharpRuntime::intcs i) override {
        data_.erase(data_.begin() + i);
        ++version_;
    }
};

}  // namespace

// ===========================================================================
// Basic indexer behaviour.
// ===========================================================================

TEST(ListIndexerBasics, GetItemReadsEveryPosition) {
    List<int> list(std::vector<int>{10, 20, 30, 40});
    EXPECT_EQ(list.getItem(0), 10);   // first
    EXPECT_EQ(list.getItem(2), 30);   // middle
    EXPECT_EQ(list.getItem(3), 40);   // last
}

TEST(ListIndexerBasics, SetItemWritesEveryPosition) {
    List<int> list(std::vector<int>{10, 20, 30, 40});
    list.setItem(0, 1);
    list.setItem(2, 3);
    list.setItem(3, 4);
    EXPECT_EQ(list.getItem(0), 1);
    EXPECT_EQ(list.getItem(1), 20);
    EXPECT_EQ(list.getItem(2), 3);
    EXPECT_EQ(list.getItem(3), 4);
    EXPECT_EQ(list.getCountProperty(), 4);
}

TEST(ListIndexerBasics, GetItemAndSetItemRejectEveryInvalidIndex) {
    List<int> list(std::vector<int>{1, 2, 3});
    const List<int>& asConst = list;

    EXPECT_THROW((void)asConst.getItem(-1), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)asConst.getItem(3), System::ArgumentOutOfRangeException);   // index == Count
    EXPECT_THROW(list.setItem(-1, 9), System::ArgumentOutOfRangeException);
    EXPECT_THROW(list.setItem(3, 9), System::ArgumentOutOfRangeException);         // index == Count

    // Nothing was written by any rejected call.
    EXPECT_EQ(list.getItem(0), 1);
    EXPECT_EQ(list.getItem(1), 2);
    EXPECT_EQ(list.getItem(2), 3);
    EXPECT_EQ(list.getCountProperty(), 3);
}

TEST(ListIndexerBasics, TheBoundsExceptionMatchesDotNetExactly) {
    // .NET validates in BOTH the getter and the setter, before any write, and
    // throws ArgumentOutOfRangeException with paramName "index" and the
    // ArgumentOutOfRange_IndexMustBeLess message (List.cs:143-164).
    List<int> list(std::vector<int>{1});
    for (int which = 0; which < 3; ++which) {
        try {
            switch (which) {
                case 0: (void)list.getItem(5); break;
                case 1: list.setItem(5, 0); break;
                default: (void)list[5]; break;
            }
            FAIL() << "expected ArgumentOutOfRangeException for case " << which;
        } catch (const System::ArgumentOutOfRangeException& ex) {
            const std::string what = ex.what();
            EXPECT_NE(what.find("Index was out of range. Must be non-negative and less than "
                                "the size of the collection."),
                      std::string::npos) << what;
            EXPECT_NE(what.find("index"), std::string::npos) << what;
        }
    }
}

TEST(ListIndexerBasics, AnEmptyListRejectsEveryIndexThroughEveryAccessor) {
    List<int> list;
    const List<int>& asConst = list;
    EXPECT_THROW((void)asConst.getItem(0), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)asConst.getItem(-1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(list.setItem(0, 1), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)list[0], System::ArgumentOutOfRangeException);
}

TEST(ListIndexerBasics, AOneElementListWorksAtItsSingleIndex) {
    List<int> list(std::vector<int>{7});
    EXPECT_EQ(list.getItem(0), 7);
    list[0] = 8;
    EXPECT_EQ(list.getItem(0), 8);
    list.setItem(0, 9);
    EXPECT_EQ(list.getItem(0), 9);
    EXPECT_THROW((void)list[1], System::ArgumentOutOfRangeException);
}

TEST(ListIndexerBasics, ItWorksForStdString) {
    List<std::string> list(std::vector<std::string>{"alpha", "beta"});
    EXPECT_EQ(list.getItem(0), "alpha");
    list[1] = "gamma";
    EXPECT_EQ(list.getItem(1), "gamma");
    list.setItem(0, std::string("delta"));
    EXPECT_EQ(list.getItem(0), "delta");
    // The proxy's own operator== restores the gtest spelling that template
    // argument deduction would otherwise reject for a proxy operand.
    EXPECT_EQ(list[0], "delta");
    EXPECT_EQ(list[1], std::string("gamma"));
}

TEST(ListIndexerBasics, ItWorksForANonTrivialCountedType) {
    Counted::reset();
    {
        List<Counted> list;
        list.Add(Counted(1, "one"));
        list.Add(Counted(2, "two"));

        EXPECT_EQ(list.getItem(0).value, 1);
        EXPECT_EQ(list.getItem(1).tag, "two");

        // Copy-modify-set: the migration for what used to be list[i].member = v.
        Counted copy = list.getItem(0);
        copy.value = 42;
        copy.tag = "changed";
        list.setItem(0, copy);

        EXPECT_EQ(list.getItem(0).value, 42);
        EXPECT_EQ(list.getItem(0).tag, "changed");
        EXPECT_EQ(list.getCountProperty(), 2);

        // Read-only member access through the proxy's operator->.
        EXPECT_EQ(list[1]->value, 2);
        EXPECT_EQ(list[0]->tag, "changed");

        // A read through the proxy constructs nothing at all.
        const int createdBefore = Counted::created();
        EXPECT_EQ(list[1]->value, 2);
        EXPECT_EQ(list.getItem(1).value, 2);
        EXPECT_EQ(Counted::created(), createdBefore);

        // A tracked replacement assigns; it does not construct a new element in place.
        const int assignedBefore = Counted::copyAssigned;
        list.setItem(1, copy);
        EXPECT_EQ(Counted::copyAssigned, assignedBefore + 1);
    }
    // Every object that was created was destroyed: the proxy owns nothing and leaks nothing.
    EXPECT_EQ(Counted::created(), Counted::destroyed);
}

TEST(ListIndexerBasics, ItWorksForSharedPtrElements) {
    List<std::shared_ptr<int>> list;
    auto first = std::make_shared<int>(1);
    auto second = std::make_shared<int>(2);
    list.Add(first);
    list.Add(second);

    EXPECT_EQ(*list.getItem(0), 1);
    EXPECT_EQ(first.use_count(), 2);  // the list holds one, `first` holds one

    // Replacing drops the list's reference to the old element.
    list[0] = second;
    EXPECT_EQ(first.use_count(), 1);
    EXPECT_EQ(second.use_count(), 3);
    EXPECT_EQ(*list.getItem(0), 2);

    // A read through the proxy must not change any ownership count.
    const long before = second.use_count();
    EXPECT_EQ(*list.getItem(1), 2);
    EXPECT_EQ(list[1]->operator*(), 2);
    EXPECT_EQ(second.use_count(), before);
}

TEST(ListIndexerBasics, MoveAssignmentThroughTheProxyMovesRatherThanCopies) {
    List<std::string> list(std::vector<std::string>{"a"});
    std::string source(64, 'x');
    const char* sourceData = source.data();

    list[0] = std::move(source);

    EXPECT_EQ(list.getItem(0).size(), 64u);
    // The heap buffer was transferred, not copied.
    EXPECT_EQ(list.getItem(0).data(), sourceData);
}

// ===========================================================================
// Versioning -- observed directly through the #1800 seam.
// ===========================================================================

TEST(ListIndexerVersioning, SetItemWithADifferentValueAdvancesExactlyOnce) {
    List<int> list(std::vector<int>{1, 2, 3});
    const auto before = versionOf(list);
    list.setItem(1, 99);
    EXPECT_EQ(versionOf(list), before + 1);
}

TEST(ListIndexerVersioning, SetItemWithAnEqualValueAdvancesToo) {
    // .NET never compares the old value (List.cs:161-162).
    List<int> list(std::vector<int>{1, 2, 3});
    const auto before = versionOf(list);
    list.setItem(1, 2);
    EXPECT_EQ(versionOf(list), before + 1);
}

TEST(ListIndexerVersioning, ProxyAssignmentAdvancesExactlyOnce) {
    List<int> list(std::vector<int>{1, 2, 3});
    const auto before = versionOf(list);
    list[1] = 99;
    EXPECT_EQ(versionOf(list), before + 1);
}

TEST(ListIndexerVersioning, EveryTrackedWriteFormAdvancesExactlyOnce) {
    List<int> list(std::vector<int>{10, 10, 10, 10, 10, 10, 10});
    auto expected = versionOf(list);

    list[0] = 1;                         expected += 1;
    EXPECT_EQ(versionOf(list), expected) << "copy assignment";
    int movable = 2;
    list[1] = std::move(movable);        expected += 1;   // move assignment form
    EXPECT_EQ(versionOf(list), expected) << "move assignment";
    list[2] = list[0];                   expected += 1;
    EXPECT_EQ(versionOf(list), expected) << "proxy-to-proxy assignment";
    list[3] += 5;                        expected += 1;
    EXPECT_EQ(versionOf(list), expected) << "compound assignment";
    ++list[4];                           expected += 1;
    EXPECT_EQ(versionOf(list), expected) << "pre-increment";
    list[5]++;                           expected += 1;
    EXPECT_EQ(versionOf(list), expected) << "post-increment";
    --list[6];                           expected += 1;
    EXPECT_EQ(versionOf(list), expected) << "pre-decrement";
    list[6]--;                           expected += 1;
    EXPECT_EQ(versionOf(list), expected) << "post-decrement";
    list.setItem(0, 7);                  expected += 1;
    EXPECT_EQ(versionOf(list), expected) << "setItem";
}

TEST(ListIndexerVersioning, ReadsAdvanceNothing) {
    List<std::string> list(std::vector<std::string>{"a", "b", "c"});
    const List<std::string>& asConst = list;
    const auto before = versionOf(list);

    (void)list.getItem(0);
    (void)asConst[1];
    (void)asConst.getItem(2);
    (void)list.ToVector().size();
    (void)list.ToArray();
    (void)list.Contains("a");
    (void)list.IndexOf("b");
    (void)list.getCountProperty();
    for (const auto& element : asConst) (void)element;
    EXPECT_EQ(list[0], "a");             // proxy conversion + comparison
    EXPECT_EQ(list[1]->size(), 1u);      // proxy operator->
    const std::string copy = list[2];    // proxy conversion to a value
    EXPECT_EQ(copy, "c");

    EXPECT_EQ(versionOf(list), before);
}

TEST(ListIndexerVersioning, MerelyAcquiringAProxyAdvancesNothing) {
    List<int> list(std::vector<int>{1, 2, 3});
    const auto before = versionOf(list);
    auto proxy = list[1];
    (void)proxy;
    auto another = list[2];
    (void)another.getValueProperty();
    EXPECT_EQ(versionOf(list), before);
}

TEST(ListIndexerVersioning, AnOutOfRangeWriteAdvancesNothing) {
    List<int> list(std::vector<int>{1, 2, 3});
    const auto before = versionOf(list);
    EXPECT_THROW(list.setItem(9, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)list[9], System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)list[-1], System::ArgumentOutOfRangeException);
    EXPECT_EQ(versionOf(list), before) << "bounds are validated before the proxy exists";
}

TEST(ListIndexerVersioning, AThrowingElementAssignmentStillInvalidates) {
    // A strengthening over .NET, which cannot reach the case: if T's own
    // assignment throws part-way, the element is left in an unspecified state,
    // so an enumerator MUST fail fast rather than read it as unchanged. The
    // counter advance therefore happens on the throwing path too.
    List<ThrowsOnAssign> list;
    list.Add(ThrowsOnAssign(1));
    list.Add(ThrowsOnAssign(2));
    const auto before = versionOf(list);

    ThrowsOnAssign::armed = true;
    EXPECT_THROW(list.setItem(0, ThrowsOnAssign(9)), System::InvalidOperationException);
    ThrowsOnAssign::armed = false;

    EXPECT_EQ(versionOf(list), before + 1);
    EXPECT_EQ(list.getItem(0).value, 1) << "the refused assignment wrote nothing";
}

TEST(ListIndexerVersioning, AnIndexWriteInvalidatesAnActiveGenericEnumerator) {
    List<int> list(std::vector<int>{1, 2, 3});
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());
    list[0] = 99;
    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
    EXPECT_THROW(e->Reset(), System::InvalidOperationException);
}

TEST(ListIndexerVersioning, AnIndexWriteInvalidatesEveryOutstandingEnumerator) {
    List<int> list(std::vector<int>{1, 2, 3});
    ScopedEnumerator<int> first(list);
    ScopedEnumerator<int> second(list);
    ASSERT_TRUE(first->MoveNext());
    ASSERT_TRUE(second->MoveNext());

    list.setItem(2, 42);

    EXPECT_THROW((void)first->MoveNext(), System::InvalidOperationException);
    EXPECT_THROW((void)second->MoveNext(), System::InvalidOperationException);
}

TEST(ListIndexerVersioning, AnEnumeratorTakenAfterTheWriteIsValid) {
    List<int> list(std::vector<int>{1, 2, 3});
    list[0] = 99;
    ScopedEnumerator<int> e(list);
    EXPECT_NO_THROW((void)e->MoveNext());
    EXPECT_EQ(e->Current(), 99);
}

TEST(ListIndexerVersioning, AReadDoesNotInvalidateAnActiveEnumerator) {
    List<int> list(std::vector<int>{1, 2, 3});
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());
    EXPECT_EQ(list.getItem(0), 1);
    EXPECT_EQ(list[1], 2);
    EXPECT_NO_THROW((void)e->MoveNext());
}

TEST(ListIndexerVersioning, ANativeIteratorIsNotVersionCheckedAndSaysSo) {
    // Determined and pinned separately: begin()/end() are STL-interop
    // extensions that follow std::vector's rules, not .NET's version-checked
    // contract. A native iterator taken before a tracked write stays usable and
    // observes the new value. That is the documented contract, not an oversight.
    List<int> list(std::vector<int>{1, 2, 3});
    auto native = list.begin();
    list[0] = 99;
    EXPECT_EQ(*native, 99);
    EXPECT_EQ(native, list.begin());
}

TEST(ListIndexerVersioning, WritingThroughAnInterfaceReferenceInvalidatesToo) {
    List<int> list(std::vector<int>{1, 2, 3});
    IList<int>& asInterface = list;
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());

    asInterface.setItem(1, 42);

    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
    EXPECT_EQ(list.getItem(1), 42);
}

TEST(ListIndexerVersioning, WritingToACopyLeavesTheOriginalEnumerable) {
    List<int> original(std::vector<int>{1, 2, 3});
    List<int> copy(original);
    ScopedEnumerator<int> e(original);
    ASSERT_TRUE(e->MoveNext());

    copy[0] = 99;
    copy.setItem(1, 98);

    EXPECT_NO_THROW((void)e->MoveNext());
    EXPECT_EQ(original.getItem(0), 1);
    EXPECT_EQ(copy.getItem(0), 99);
}

// ===========================================================================
// The proxy's own C++ behaviour.
// ===========================================================================

TEST(ListIndexerProxyLanguage, TheProxyIsTwoPointersAndOwnsNothing) {
    static_assert(sizeof(ElementReference<int>) == 2 * sizeof(void*));
    static_assert(alignof(ElementReference<int>) == alignof(void*));
    static_assert(sizeof(ElementReference<std::string>) == 2 * sizeof(void*));
    static_assert(std::is_nothrow_copy_constructible_v<ElementReference<int>>);
    static_assert(std::is_trivially_destructible_v<ElementReference<int>>);
    SUCCEED();
}

TEST(ListIndexerProxyLanguage, DeclaredTypesAreExactlyWhatTheDesignSelected) {
    static_assert(std::is_same_v<decltype(std::declval<List<int>&>()[0]),
                                 ElementReference<int>>);
    static_assert(std::is_same_v<decltype(std::declval<const List<int>&>()[0]), const int&>);
    static_assert(std::is_same_v<decltype(std::declval<List<int>&>().getItem(0)), const int&>);
    static_assert(std::is_same_v<decltype(std::declval<IList<int>&>()[0]),
                                 ElementReference<int>>);
    static_assert(std::is_same_v<decltype(std::declval<Collection<int>&>()[0]),
                                 ElementReference<int>>);
    static_assert(std::is_same_v<decltype(std::declval<ReadOnlyCollection<int>&>()[0]),
                                 ElementReference<int>>);
    SUCCEED();
}

TEST(ListIndexerProxyLanguage, AutoCopiesTheAliasAndConstAutoReadsThrough) {
    List<int> list(std::vector<int>{1, 2, 3});

    // `auto` deduces the proxy itself: an alias, not a value.
    auto aliasing = list[0];
    static_assert(std::is_same_v<decltype(aliasing), ElementReference<int>>);
    EXPECT_EQ(aliasing, 1);

    // Assigning through the copied alias still writes, and still tracks.
    const auto before = versionOf(list);
    aliasing = 11;
    EXPECT_EQ(versionOf(list), before + 1);
    EXPECT_EQ(list.getItem(0), 11);

    // `const auto` is a const proxy: reads work, writes do not compile.
    const auto readOnly = list[1];
    EXPECT_EQ(readOnly, 2);
    EXPECT_EQ(readOnly.getValueProperty(), 2);

    // `const auto&` binding to the element by value conversion.
    const int& element = list[2];
    EXPECT_EQ(element, 3);

    // decltype(auto) preserves the proxy, so it stays assignable.
    decltype(auto) preserved = list[2];
    static_assert(std::is_same_v<decltype(preserved), ElementReference<int>>);
    preserved = 33;
    EXPECT_EQ(list.getItem(2), 33);
}

TEST(ListIndexerProxyLanguage, ConversionToAValueCopies) {
    List<std::string> list(std::vector<std::string>{"alpha"});
    const std::string byValue = list[0];             // proxy -> value
    EXPECT_EQ(byValue, "alpha");

    list[0] = "beta";
    EXPECT_EQ(byValue, "alpha") << "the copy is detached from the list";
    EXPECT_EQ(list.getItem(0), "beta");
}

TEST(ListIndexerProxyLanguage, ValueToProxyAndProxyToProxyAssignBothWrite) {
    List<int> list(std::vector<int>{1, 2, 3});

    list[0] = 10;                 // value -> proxy
    EXPECT_EQ(list.getItem(0), 10);

    list[1] = list[0];            // proxy -> proxy, assigns THROUGH
    EXPECT_EQ(list.getItem(1), 10);
    EXPECT_EQ(list.getItem(0), 10) << "the source element is untouched";

    // A proxy is never rebound: assigning one proxy from another for a
    // different list copies the VALUE across, it does not alias the other list.
    List<int> other(std::vector<int>{77});
    auto here = list[2];
    auto there = other[0];
    here = there;
    EXPECT_EQ(list.getItem(2), 77);
    other.setItem(0, 88);
    EXPECT_EQ(list.getItem(2), 77) << "list[2] never became an alias into `other`";
}

TEST(ListIndexerProxyLanguage, AssigningAConvertibleValueMaterialisesNoTemporary) {
    // The proxy forwards any type the element is assignable from straight to the
    // element's own assignment. Without that, `list[i] = "abc"` would have to
    // build a temporary std::string first, at one heap allocation per write.
    List<std::string> list(std::vector<std::string>{"a", "b"});

    list[0] = "a rather long string, comfortably past the SSO boundary";
    EXPECT_EQ(list.getItem(0), "a rather long string, comfortably past the SSO boundary");

    const auto before = versionOf(list);
    list[1] = "xyz";
    EXPECT_EQ(list.getItem(1), "xyz");
    EXPECT_EQ(versionOf(list), before + 1) << "a forwarded assignment is still tracked once";

    // It must not displace the assign-through proxy-to-proxy overload.
    list[0] = list[1];
    EXPECT_EQ(list.getItem(0), "xyz");

    // Nor the exact-T overloads.
    const std::string value = "exact";
    list[0] = value;
    EXPECT_EQ(list.getItem(0), "exact");
    std::string movable = "moved";
    list[1] = std::move(movable);
    EXPECT_EQ(list.getItem(1), "moved");
}

TEST(ListIndexerProxyLanguage, SelfAssignmentIsSafeAndStillAdvances) {
    List<std::string> list(std::vector<std::string>{"alpha", "beta"});
    const auto before = versionOf(list);

    list[0] = list[0];

    EXPECT_EQ(list.getItem(0), "alpha");
    EXPECT_EQ(versionOf(list), before + 1) << ".NET bumps unconditionally";
}

TEST(ListIndexerProxyLanguage, CompoundAssignmentsForwardAndTrack) {
    List<int> list(std::vector<int>{100, 100, 100, 100, 0b1100, 0b1100, 0b1100, 1, 64});
    list[0] += 5;   EXPECT_EQ(list.getItem(0), 105);
    list[1] -= 5;   EXPECT_EQ(list.getItem(1), 95);
    list[2] *= 2;   EXPECT_EQ(list.getItem(2), 200);
    list[3] /= 4;   EXPECT_EQ(list.getItem(3), 25);
    list[4] &= 0b1010; EXPECT_EQ(list.getItem(4), 0b1000);
    list[5] |= 0b0011; EXPECT_EQ(list.getItem(5), 0b1111);
    list[6] ^= 0b0101; EXPECT_EQ(list.getItem(6), 0b1001);
    list[7] <<= 3;  EXPECT_EQ(list.getItem(7), 8);
    list[8] >>= 2;  EXPECT_EQ(list.getItem(8), 16);

    List<int> mod(std::vector<int>{17});
    mod[0] %= 5;    EXPECT_EQ(mod.getItem(0), 2);

    List<std::string> text(std::vector<std::string>{"a"});
    text[0] += "bc";
    EXPECT_EQ(text.getItem(0), "abc");
}

TEST(ListIndexerProxyLanguage, IncrementAndDecrementReturnTheRightValues) {
    List<int> list(std::vector<int>{5, 5});

    EXPECT_EQ(list[0]++, 5) << "post-increment yields the value before";
    EXPECT_EQ(list.getItem(0), 6);
    EXPECT_EQ(list[1]--, 5) << "post-decrement yields the value before";
    EXPECT_EQ(list.getItem(1), 4);

    ++list[0];
    EXPECT_EQ(list.getItem(0), 7);
    --list[1];
    EXPECT_EQ(list.getItem(1), 3);

    // Post-increment returns a detached value, never an alias.
    static_assert(std::is_same_v<decltype(std::declval<List<int>&>()[0]++), int>);
}

TEST(ListIndexerProxyLanguage, ComparisonWorksInBothOperandPositions) {
    List<int> list(std::vector<int>{10, 20});
    EXPECT_TRUE(list[0] == 10);
    EXPECT_TRUE(10 == list[0]);
    EXPECT_TRUE(list[0] != 20);
    EXPECT_TRUE(list[0] == list[0]);
    EXPECT_TRUE(list[0] != list[1]);
    // Ordering resolves through the conversion to const T& and the built-in operators.
    EXPECT_TRUE(list[0] < list[1]);
    EXPECT_TRUE(list[1] > 15);
    EXPECT_EQ(list[0] + list[1], 30);
    EXPECT_EQ(list[0] > 5 ? 1 : 0, 1);

    List<std::string> text(std::vector<std::string>{"abc"});
    EXPECT_TRUE(text[0] == "abc");
    EXPECT_TRUE(std::string("abc") == text[0]);
}

TEST(ListIndexerProxyLanguage, GenericCodeStillDeducesThroughTheProxy) {
    List<int> list(std::vector<int>{3, 9});
    auto takesConstRef = [](const int& v) { return v * 2; };
    EXPECT_EQ(takesConstRef(list[0]), 6);

    // A conversion to const T& means an ordinary read-only API is unaffected.
    const int copy = list[1];
    EXPECT_EQ(copy, 9);
}

TEST(ListIndexerProxyLanguage, AProxyOverAMovedFromListStillTracksItsOwnStorage) {
    // The lifetime contract: a proxy is valid for the full-expression that
    // created it. This pins what a stored proxy does across a move, which is
    // the shape most likely to be reached by accident.
    List<int> source(std::vector<int>{1, 2, 3});
    const List<int> moved(std::move(source));
    EXPECT_EQ(moved.getItem(0), 1);

    // A freshly taken proxy over the destination works normally.
    List<int> destination(std::vector<int>{7});
    const auto before = versionOf(destination);
    destination[0] = 8;
    EXPECT_EQ(versionOf(destination), before + 1);
    EXPECT_EQ(destination.getItem(0), 8);
}

TEST(ListIndexerProxyLanguage, ATrackedWriteThroughAProxyIsVisibleThroughEveryReader) {
    List<int> list(std::vector<int>{1, 2, 3});
    const List<int>& asConst = list;
    IList<int>& asInterface = list;

    list[1] = 42;

    EXPECT_EQ(asConst[1], 42);
    EXPECT_EQ(asConst.getItem(1), 42);
    EXPECT_EQ(asInterface.getItem(1), 42);
    EXPECT_EQ(list.ToVector()[1], 42);
    EXPECT_EQ(list.ToArray()[1], 42);
}

// ===========================================================================
// The closed ToVector() escape.
// ===========================================================================

TEST(ListIndexerToVector, NoMutableBackingVectorRemainsObtainable) {
    // Both overloads now yield const std::vector<T>&, on a const AND a non-const
    // List<T>. There is deliberately no renamed mutable replacement.
    static_assert(std::is_same_v<decltype(std::declval<List<int>&>().ToVector()),
                                 const std::vector<int>&>);
    static_assert(std::is_same_v<decltype(std::declval<const List<int>&>().ToVector()),
                                 const std::vector<int>&>);
    SUCCEED();
}

TEST(ListIndexerToVector, ReadingThroughItStillWorksAndStaysContiguous) {
    List<int> list(std::vector<int>{1, 2, 3});
    const std::vector<int>& view = list.ToVector();
    EXPECT_EQ(view.size(), 3u);
    EXPECT_EQ(view[2], 3);
    // Contiguity is preserved, so a const T* is still reachable for interop.
    const int* data = list.ToVector().data();
    EXPECT_EQ(data[0], 1);
    EXPECT_EQ(data[2], 3);
}

TEST(ListIndexerToVector, ToArrayReturnsADetachedCopy) {
    List<int> list(std::vector<int>{1, 2, 3});
    std::vector<int> snapshot = list.ToArray();
    snapshot.push_back(4);
    snapshot[0] = 99;

    EXPECT_EQ(list.getCountProperty(), 3) << "the copy is not the list";
    EXPECT_EQ(list.getItem(0), 1);
    EXPECT_EQ(snapshot.size(), 4u);
}

TEST(ListIndexerToVector, StructuralMutationNowRequiresATrackedMethodAndInvalidates) {
    List<int> list(std::vector<int>{1, 2, 3});
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());

    // Reading the backing vector invalidates nothing.
    EXPECT_EQ(list.ToVector().size(), 3u);
    EXPECT_NO_THROW((void)e->MoveNext());

    // Every structural route is tracked.
    const auto before = versionOf(list);
    list.Add(4);
    EXPECT_EQ(versionOf(list), before + 1);
    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
    EXPECT_EQ(list.getCountProperty(), 4);
}

TEST(ListIndexerToVector, EnumeratorsAreInvalidatedOnlyByEffectiveMutations) {
    List<int> list(std::vector<int>{1, 2, 3});
    {
        ScopedEnumerator<int> e(list);
        ASSERT_TRUE(e->MoveNext());
        EXPECT_FALSE(list.Remove(99));   // no such element: nothing erased
        EXPECT_NO_THROW((void)e->MoveNext()) << "a Remove that erased nothing must not bump";
    }
    {
        ScopedEnumerator<int> e(list);
        ASSERT_TRUE(e->MoveNext());
        EXPECT_EQ(list.RemoveAll([](const int& v) { return v > 100; }), 0);
        EXPECT_NO_THROW((void)e->MoveNext()) << "a RemoveAll that removed nothing must not bump";
    }
    {
        ScopedEnumerator<int> e(list);
        ASSERT_TRUE(e->MoveNext());
        EXPECT_TRUE(list.Remove(1));
        EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
    }
}

// ===========================================================================
// All four IList<T> implementers.
// ===========================================================================

TEST(ListIndexerImplementers, ListTracksReadsAndWrites) {
    List<int> list(std::vector<int>{1, 2, 3});
    IList<int>& asInterface = list;
    EXPECT_EQ(asInterface.getItem(0), 1);
    asInterface.setItem(0, 9);
    EXPECT_EQ(asInterface.getItem(0), 9);
    asInterface[1] = 8;
    EXPECT_EQ(asInterface.getItem(1), 8);
}

TEST(ListIndexerImplementers, CollectionTracksAnIndexedWriteAndFailsFast) {
    Collection<int> collection;
    collection.Add(1);
    collection.Add(2);

    const auto before = versionOf(collection);
    collection[0] = 99;
    EXPECT_EQ(versionOf(collection), before + 1);
    EXPECT_EQ(collection.getItem(0), 99);

    IEnumerator<int>* e = collection.GetEnumerator();
    ASSERT_TRUE(e->MoveNext());
    collection.setItem(1, 42);
    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
    delete e;

    EXPECT_EQ(collection.getItem(1), 42);
}

TEST(ListIndexerImplementers, CollectionStructuralMutationAlsoFailsFast) {
    // Before #1791 Collection<T> had no counter at all, so not even Add()
    // invalidated an outstanding enumerator. It does now, matching .NET, whose
    // Collection<T> delegates to the wrapped List<T>'s fail-fast enumerator.
    Collection<int> collection;
    collection.Add(1);
    collection.Add(2);

    for (int which = 0; which < 3; ++which) {
        Collection<int> c;
        c.Add(1);
        c.Add(2);
        IEnumerator<int>* e = c.GetEnumerator();
        ASSERT_TRUE(e->MoveNext());
        switch (which) {
            case 0: c.Add(3); break;
            case 1: c.Insert(0, 9); break;
            default: c.RemoveAt(0); break;
        }
        EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException) << "case " << which;
        delete e;
    }
}

TEST(ListIndexerImplementers, CollectionSetItemRunsTheOverridableHook) {
    // setItem dispatches through the virtual SetItem hook, which the plain
    // indexer cannot do -- the documented, narrowed residual.
    struct HookCounting : Collection<int> {
        int hookCalls = 0;
    protected:
        void SetItem(SharpRuntime::intcs index, const int& item) override {
            ++hookCalls;
            Collection<int>::SetItem(index, item);
        }
    };

    HookCounting collection;
    collection.Add(1);
    collection.Add(2);

    collection.setItem(0, 10);
    EXPECT_EQ(collection.hookCalls, 1) << "setItem must run the hook";
    EXPECT_EQ(collection.getItem(0), 10);

    const int callsBefore = collection.hookCalls;
    collection[1] = 20;
    EXPECT_EQ(collection.hookCalls, callsBefore)
        << "the plain indexer still cannot make a virtual call -- documented in Collection.hpp";
    EXPECT_EQ(collection.getItem(1), 20) << "but the write is now tracked";
}

TEST(ListIndexerImplementers, ReadOnlyCollectionReadsButRefusesEveryWrite) {
    ReadOnlyCollection<int> readOnly(std::vector<int>{1, 2, 3});
    const ReadOnlyCollection<int>& asConst = readOnly;

    // Reading is supported through every read accessor.
    EXPECT_EQ(asConst[0], 1);
    EXPECT_EQ(asConst.getItem(2), 3);
    EXPECT_EQ(readOnly.getItem(1), 2);
    EXPECT_EQ(readOnly.getCountProperty(), 3);
    EXPECT_TRUE(readOnly.getIsReadOnlyProperty());

    // Every mutation path throws, and no mutable alias escapes.
    EXPECT_THROW((void)readOnly[0], System::NotSupportedException);
    EXPECT_THROW(readOnly.setItem(0, 9), System::NotSupportedException);
    EXPECT_THROW(readOnly.Add(4), System::NotSupportedException);
    EXPECT_THROW(readOnly.Clear(), System::NotSupportedException);

    // Through the interface too.
    IList<int>& asInterface = readOnly;
    EXPECT_THROW((void)asInterface[0], System::NotSupportedException);
    EXPECT_THROW(asInterface.setItem(0, 9), System::NotSupportedException);
    EXPECT_EQ(asInterface.getItem(0), 1);

    // Nothing was written by any of it.
    EXPECT_EQ(asConst.getItem(0), 1);
    EXPECT_EQ(readOnly.getCountProperty(), 3);
}

TEST(ListIndexerImplementers, TheHandWrittenImplementerTracksItsOwnCounter) {
    HandWrittenList list;
    list.Add(1);
    list.Add(2);
    IList<int>& asInterface = list;

    EXPECT_EQ(asInterface.getItem(0), 1);
    asInterface[0] = 9;
    EXPECT_EQ(asInterface.getItem(0), 9);
    asInterface.setItem(1, 8);
    EXPECT_EQ(asInterface.getItem(1), 8);

    // The migration a hand-written implementer must perform is exactly this:
    // one return-type change, two new methods, and a counter to hand the proxy.
    static_assert(std::is_same_v<decltype(std::declval<HandWrittenList&>()[0]),
                                 ElementReference<int>>);
}

TEST(ListIndexerImplementers, EveryImplementerAgreesOnTheReadContract) {
    List<int> list(std::vector<int>{5});
    Collection<int> collection;
    collection.Add(5);
    ReadOnlyCollection<int> readOnly(std::vector<int>{5});
    HandWrittenList handWritten;
    handWritten.Add(5);

    IList<int>* implementers[] = {&list, &collection, &readOnly, &handWritten};
    for (IList<int>* implementer : implementers) {
        EXPECT_EQ(implementer->getItem(0), 5);
        EXPECT_EQ(implementer->getCountProperty(), 1);
    }
}
