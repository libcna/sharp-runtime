// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1790
// (REMED-COLL-LIST-INDEXER-VERSION), design record
// docs/ListIndexerVersioningDesign.md.
//
// #1790 is a DESIGN ticket: it changes no production behaviour. This suite
// therefore pins the behaviour that exists TODAY, in two deliberately separated
// groups:
//
//   * ListIndexerVersionContract  -- behaviour that must SURVIVE the future
//     implementation ticket #1791 unchanged (reads never invalidate, structural
//     mutations always do, a write to a copy never disturbs the original, the
//     bounds/exception contract, const-correctness).
//
//   * ListIndexerVersionDivergence -- the measured divergence from .NET that
//     #1791 exists to close. Every case here is written as an assertion about
//     what this port does now, with the .NET behaviour named in a comment, so
//     that #1791 must consciously FLIP it rather than silently drift past it.
//     These are the cases the ticket's acceptance criteria require ("permanent
//     regressions must cover a value-only index write during enumeration in
//     either case").
//
// Real .NET's List<T> index setter bumps _version unconditionally
// (List.cs:155-163), so `list[i] = v` fails an in-progress enumeration fast.
// This port's operator[] returns a plain T&, which no member can intercept, so
// it does not. See the design record for the full alternatives analysis.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/detail/ElementReference.hpp"
#include "System/InvalidOperationException.hpp"

using System::Collections::Generic::IEnumerator;
using System::Collections::Generic::IList;
using System::Collections::Generic::List;

namespace {

/** Owns the enumerator GetEnumerator() hands back, which is caller-owned. */
template <typename T>
class ScopedEnumerator {
    IEnumerator<T>* e_;
public:
    explicit ScopedEnumerator(List<T>& list) : e_(list.GetEnumerator()) {}
    ScopedEnumerator(const ScopedEnumerator&) = delete;
    ScopedEnumerator& operator=(const ScopedEnumerator&) = delete;
    ~ScopedEnumerator() { delete e_; }
    IEnumerator<T>* operator->() const { return e_; }
};

}  // namespace

// ---------------------------------------------------------------------------
// Contract -- must survive ticket #1791 unchanged.
// ---------------------------------------------------------------------------

TEST(ListIndexerVersionContract, ReadingThroughTheIndexerNeverInvalidates) {
    List<int> list(std::vector<int>{10, 20, 30});
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());
    // Reads through both overloads, on a non-const and a const path.
    EXPECT_EQ(list[0], 10);
    const List<int>& asConst = list;
    EXPECT_EQ(asConst[2], 30);
    EXPECT_NO_THROW((void)e->MoveNext());
}

TEST(ListIndexerVersionContract, StructuralMutationStillInvalidates) {
    // The guard itself works; only the indexer write bypasses it. Each of these
    // must keep throwing after #1791.
    {
        List<int> list(std::vector<int>{1, 2});
        ScopedEnumerator<int> e(list);
        ASSERT_TRUE(e->MoveNext());
        list.Add(3);
        EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
    }
    {
        List<int> list(std::vector<int>{1, 2});
        ScopedEnumerator<int> e(list);
        ASSERT_TRUE(e->MoveNext());
        list.Insert(0, 9);
        EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
    }
    {
        List<int> list(std::vector<int>{1, 2});
        ScopedEnumerator<int> e(list);
        ASSERT_TRUE(e->MoveNext());
        list.RemoveAt(0);
        EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
    }
    {
        List<int> list(std::vector<int>{1, 2});
        ScopedEnumerator<int> e(list);
        ASSERT_TRUE(e->MoveNext());
        list.Clear();
        EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
    }
}

TEST(ListIndexerVersionContract, IndexerBoundsContractMatchesDotNet) {
    // .NET throws ArgumentOutOfRangeException with paramName "index" and the
    // ArgumentOutOfRange_IndexMustBeLess message, from BOTH the getter and the
    // setter, and validates BEFORE writing anything (List.cs:143-164).
    List<int> list(std::vector<int>{1, 2, 3});
    const List<int>& asConst = list;

    EXPECT_THROW((void)list[-1], System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)list[3], System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)asConst[-1], System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)asConst[3], System::ArgumentOutOfRangeException);

    try {
        (void)list[3];
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& ex) {
        const std::string what = ex.what();
        EXPECT_NE(what.find("Index was out of range"), std::string::npos) << what;
        EXPECT_NE(what.find("index"), std::string::npos) << what;
    }

    // An out-of-range write leaves every element untouched.
    EXPECT_EQ(list[0], 1);
    EXPECT_EQ(list[1], 2);
    EXPECT_EQ(list[2], 3);
    EXPECT_EQ(list.getCountProperty(), 3);
}

TEST(ListIndexerVersionContract, AnEmptyListRejectsEveryIndex) {
    List<int> list;
    EXPECT_THROW((void)list[0], System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)list[-1], System::ArgumentOutOfRangeException);
}

TEST(ListIndexerVersionContract, WritingToACopyLeavesTheOriginalEnumerable) {
    // Independent value semantics: the copy owns its own storage AND its own
    // counter, so mutating one must never invalidate an enumerator over the
    // other. This must hold whatever #1791 does to the indexer.
    List<int> original(std::vector<int>{1, 2, 3});
    List<int> copy(original);
    ScopedEnumerator<int> e(original);
    ASSERT_TRUE(e->MoveNext());

    copy[0] = 99;

    EXPECT_NO_THROW((void)e->MoveNext());
    EXPECT_EQ(original[0], 1);
    EXPECT_EQ(copy[0], 99);
}

TEST(ListIndexerVersionContract, AssignmentStillInvalidatesTheDestination) {
    // Ticket #1787's repair. Unrelated to the indexer, and must stay.
    List<int> a(std::vector<int>{1, 2, 3});
    const List<int> b(std::vector<int>{9});
    ScopedEnumerator<int> e(a);
    ASSERT_TRUE(e->MoveNext());
    a = b;
    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
}

TEST(ListIndexerVersionContract, ConstIndexerReturnsAConstReference) {
    // A const List<T> exposes no write path at all, before or after #1791.
    static_assert(std::is_same_v<decltype(std::declval<const List<int>&>()[0]),
                                 const int&>,
                  "const List<T>::operator[] must return const T&");
    static_assert(std::is_same_v<decltype(std::declval<const List<int>&>().ToVector()),
                                 const std::vector<int>&>,
                  "const ToVector() must return const std::vector<T>&");
    const List<int> list(std::vector<int>{7});
    EXPECT_EQ(list[0], 7);
}

TEST(ListIndexerVersionContract, IndexWritesAreVisibleAndDoNotChangeCount) {
    // Whatever the indexer returns, replacement must replace and must not
    // resize. This is the half of .NET's setter contract this port already has.
    List<std::string> list(std::vector<std::string>{"a", "b", "c"});
    list[1] = "replaced";
    EXPECT_EQ(list[1], "replaced");
    EXPECT_EQ(list.getCountProperty(), 3);
    EXPECT_EQ(list[0], "a");
    EXPECT_EQ(list[2], "c");
}

// ---------------------------------------------------------------------------
// Divergence -- FLIPPED by ticket #1791.
//
// Every case below was written by #1790 as an assertion about the divergence
// this port had from .NET, with EXPECT_NO_THROW where .NET throws, so that the
// implementation ticket could not land silently. #1791 flipped each one rather
// than deleting it, so the git history of this file is the record of the
// behaviour change. The suite name is kept for that traceability.
//
// Two cases did NOT flip, and say so explicitly: the STL-interop begin()/end()
// escape, which is a deliberate documented residual, and the ToVector() case,
// whose mutable overload #1791 removed outright rather than tracked.
// ---------------------------------------------------------------------------

TEST(ListIndexerVersionDivergence, IndexWriteNowInvalidatesAnEnumerator) {
    // FLIPPED by #1791. .NET: List.cs:155-163 bumps _version in the setter, so
    // this throws; before #1791 operator[] returned a plain T& that nothing
    // could intercept, and it did not.
    List<int> list(std::vector<int>{10, 20, 30});
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());

    list[0] = 99;

    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException)
        << "ticket #1791: a value-only index write must fail an in-progress enumeration fast";
    EXPECT_EQ(list[0], 99);
}

TEST(ListIndexerVersionDivergence, EqualValueIndexWriteNowInvalidates) {
    // FLIPPED by #1791. .NET bumps _version unconditionally -- it does NOT
    // compare the old value (List.cs:161-162), so an equal-value write
    // invalidates exactly like a changing one.
    List<int> list(std::vector<int>{10, 20, 30});
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());

    list[1] = 20;  // same value

    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException)
        << "ticket #1791: .NET bumps even when the value is unchanged";
    EXPECT_EQ(list[1], 20);
}

TEST(ListIndexerVersionDivergence, IndexWriteThroughTheInterfaceNowInvalidates) {
    // FLIPPED by #1791. The same hole was reachable through IList<T>&, whose
    // operator[] also returned a plain T&; #1791's scope therefore included the
    // interface, not only List<T>.
    List<int> list(std::vector<int>{10, 20, 30});
    IList<int>& asInterface = list;
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());

    asInterface[2] = 77;

    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException)
        << "ticket #1791: IList<T>::operator[] returns the tracked proxy too";
    EXPECT_EQ(list[2], 77);
}

TEST(ListIndexerVersionDivergence, StlInteropEscapesStillDoNotInvalidate) {
    // NOT flipped, deliberately. begin()/end() are documented STL-interop
    // extensions with no .NET counterpart that follow std::vector rules, not
    // .NET's version-checked contract. #1791 kept them, because constraining
    // them would break the interop List<T> exists to provide -- .NET keeps an
    // equivalent untracked hatch (CollectionsMarshal.AsSpan) for the same
    // reason. This is THE remaining ordinary route to a mutable T& into the
    // storage, and it is pinned here so it cannot be forgotten or quietly
    // claimed as closed.
    List<int> list(std::vector<int>{1, 2, 3});
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());

    *list.begin() = 55;

    EXPECT_NO_THROW((void)e->MoveNext())
        << "ticket #1791: the STL-interop surface remains deliberately untracked";
    EXPECT_EQ(list[0], 55);
}

TEST(ListIndexerVersionDivergence, ToVectorNoLongerPermitsStructuralMutation) {
    // FLIPPED by #1791, by REMOVAL rather than by tracking. #1790 discovered
    // that the non-const ToVector() handed out the whole backing container, so
    // a caller could push_back/clear through it -- a STRUCTURAL mutation the
    // fail-fast guard never saw, strictly wider than the indexer hole. #1791
    // deleted that overload; only the const one remains, and the compile-time
    // proof is the static_assert below plus
    // test/consumer/collections_list_indexer_negative.cpp.
    static_assert(std::is_same_v<decltype(std::declval<List<int>&>().ToVector()),
                                 const std::vector<int>&>,
                  "ticket #1791: ToVector() must return const std::vector<T>& on a "
                  "non-const List<T> too -- the mutable overload is gone");

    List<int> list(std::vector<int>{1, 2, 3});
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());

    // Reading through it is still supported and still invalidates nothing.
    EXPECT_EQ(list.ToVector().size(), 3u);
    EXPECT_NO_THROW((void)e->MoveNext());

    // Structural mutation now requires a tracked List<T> method, which does
    // invalidate.
    list.Add(4);
    EXPECT_THROW((void)e->MoveNext(), System::InvalidOperationException);
    EXPECT_EQ(list.getCountProperty(), 4);
}

TEST(ListIndexerVersionDivergence, TheIndexerNoLongerHandsOutAPlainMutableReference) {
    // FLIPPED by #1791. The exact shape of the defect, pinned as a type-level
    // fact so the change could not land without this assertion being updated.
    static_assert(
        std::is_same_v<decltype(std::declval<List<int>&>()[0]),
                       System::Collections::detail::ElementReference<int>>,
        "ticket #1791: non-const List<T>::operator[] must return the tracked proxy");
    static_assert(!std::is_reference_v<decltype(std::declval<List<int>&>()[0])>,
                  "ticket #1791: the tracked indexer must yield a prvalue, not a reference");
    static_assert(std::is_same_v<decltype(std::declval<const List<int>&>()[0]), const int&>,
                  "ticket #1791: the const indexer is unchanged");

    // The proxy is two pointers and never outlives its full-expression.
    static_assert(sizeof(System::Collections::detail::ElementReference<int>) == 2 * sizeof(void*),
                  "ticket #1791: the proxy is a slot pointer plus a counter pointer");

    // A retained plain T& into the storage is no longer obtainable here -- that
    // is what removes the four reproduced use-after-free shapes of #1790 §5.3
    // from the ordinary surface. Writing still works, and is now tracked.
    List<int> list(std::vector<int>{1, 2, 3});
    list[1] = 42;
    EXPECT_EQ(list[1], 42);
}
