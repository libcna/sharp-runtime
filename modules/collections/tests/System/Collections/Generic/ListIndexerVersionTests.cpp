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
// Divergence -- ticket #1791 must deliberately flip each of these.
// ---------------------------------------------------------------------------

TEST(ListIndexerVersionDivergence, IndexWriteDoesNotYetInvalidateAnEnumerator) {
    // .NET: List.cs:155-163 bumps _version in the setter, so this throws.
    // Here: operator[] returns a plain T&, nothing intercepts the assignment.
    // #1791 must change EXPECT_NO_THROW to EXPECT_THROW.
    List<int> list(std::vector<int>{10, 20, 30});
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());

    list[0] = 99;

    EXPECT_NO_THROW((void)e->MoveNext())
        << "ticket #1790: a value-only index write is invisible to the fail-fast guard";
    EXPECT_EQ(list[0], 99);
}

TEST(ListIndexerVersionDivergence, EqualValueIndexWriteDoesNotYetInvalidate) {
    // .NET bumps _version unconditionally -- it does NOT compare the old value.
    // Recorded separately because #1791 must decide this case explicitly.
    List<int> list(std::vector<int>{10, 20, 30});
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());

    list[1] = 20;  // same value

    EXPECT_NO_THROW((void)e->MoveNext())
        << "ticket #1790: .NET bumps even when the value is unchanged";
}

TEST(ListIndexerVersionDivergence, IndexWriteThroughTheInterfaceDoesNotYetInvalidate) {
    // The same hole is reachable through IList<T>&, whose operator[] is also
    // declared to return a plain T&. #1791's scope therefore includes the
    // interface, not only List<T>.
    List<int> list(std::vector<int>{10, 20, 30});
    IList<int>& asInterface = list;
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());

    asInterface[2] = 77;

    EXPECT_NO_THROW((void)e->MoveNext())
        << "ticket #1790: IList<T>::operator[] returns T& as well";
    EXPECT_EQ(list[2], 77);
}

TEST(ListIndexerVersionDivergence, StlInteropEscapesDoNotYetInvalidate) {
    // begin()/end() are documented STL-interop extensions that follow
    // std::vector rules, not .NET's. Pinned so #1791 records a decision about
    // them rather than overlooking them.
    List<int> list(std::vector<int>{1, 2, 3});
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());

    *list.begin() = 55;

    EXPECT_NO_THROW((void)e->MoveNext());
    EXPECT_EQ(list[0], 55);
}

TEST(ListIndexerVersionDivergence, ToVectorPermitsUntrackedStructuralMutation) {
    // Discovered while investigating #1790 and NOT previously documented: the
    // non-const ToVector() hands out the whole backing container, so a caller
    // can push_back/clear through it. That is a STRUCTURAL mutation the
    // fail-fast guard never sees -- strictly wider than the indexer hole, which
    // can only replace an existing element.
    List<int> list(std::vector<int>{1, 2, 3});
    ScopedEnumerator<int> e(list);
    ASSERT_TRUE(e->MoveNext());

    list.ToVector().push_back(4);

    EXPECT_NO_THROW((void)e->MoveNext())
        << "ticket #1790: a structural mutation through ToVector() is untracked";
    EXPECT_EQ(list.getCountProperty(), 4);
}

TEST(ListIndexerVersionDivergence, TheIndexerStillHandsOutAPlainMutableReference) {
    // The exact shape of the defect, pinned as a type-level fact so #1791
    // cannot land without this assertion being updated.
    static_assert(std::is_same_v<decltype(std::declval<List<int>&>()[0]), int&>,
                  "ticket #1790: non-const List<T>::operator[] still returns a plain T&");
    static_assert(std::is_same_v<decltype(std::declval<List<int>&>().ToVector()),
                                 std::vector<int>&>,
                  "ticket #1790: non-const ToVector() still returns the backing container");

    // ...and that reference outlives nothing: it is invalidated by any
    // reallocation, exactly as std::vector's rules say. Retaining one across a
    // mutation is undefined behaviour, reproduced under AddressSanitizer in
    // build-probe-listindexer/probe1_asan_*.log. Nothing is retained here.
    List<int> list(std::vector<int>{1, 2, 3});
    int& slot = list[1];
    slot = 42;
    EXPECT_EQ(list[1], 42);
}
