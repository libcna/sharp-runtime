// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <functional>
#include <string>
#include <vector>
#include "System/ArgumentNullException.hpp"
#include "System/Exception.hpp"
#include "System/Linq.hpp"
#include "System/InvalidOperationException.hpp"

using namespace System::Linq;

// --- Where ---

TEST(LinqTests, Where_FiltersCorrectly) {
    std::vector<int> v{1, 2, 3, 4, 5};
    auto r = Where<int>(v, [](const int& x) { return x % 2 == 0; });
    ASSERT_EQ(r.size(), 2u);
    EXPECT_EQ(r[0], 2);
    EXPECT_EQ(r[1], 4);
}

TEST(LinqTests, Where_NoneMatch_EmptyResult) {
    std::vector<int> v{1, 3, 5};
    auto r = Where<int>(v, [](const int& x) { return x % 2 == 0; });
    EXPECT_TRUE(r.empty());
}

TEST(LinqTests, Where_AllMatch) {
    std::vector<int> v{2, 4, 6};
    auto r = Where<int>(v, [](const int& x) { return x % 2 == 0; });
    EXPECT_EQ(r.size(), v.size());
}

// --- Select ---

TEST(LinqTests, Select_TransformsElements) {
    std::vector<int> v{1, 2, 3};
    auto r = Select<int, int>(v, [](const int& x) { return x * x; });
    ASSERT_EQ(r.size(), 3u);
    EXPECT_EQ(r[0], 1);
    EXPECT_EQ(r[1], 4);
    EXPECT_EQ(r[2], 9);
}

TEST(LinqTests, Select_IntToString) {
    std::vector<int> v{1, 2, 3};
    auto r = Select<int, std::string>(v, [](const int& x) { return std::to_string(x); });
    EXPECT_EQ(r[0], "1");
    EXPECT_EQ(r[2], "3");
}

// --- FirstOrDefault ---

TEST(LinqTests, FirstOrDefault_WithPredicate_Found) {
    std::vector<int> v{1, 2, 3, 4};
    EXPECT_EQ(FirstOrDefault<int>(v, [](const int& x) { return x > 2; }), 3);
}

TEST(LinqTests, FirstOrDefault_WithPredicate_NotFound) {
    std::vector<int> v{1, 2, 3};
    EXPECT_EQ(FirstOrDefault<int>(v, [](const int& x) { return x > 10; }), 0);
}

TEST(LinqTests, FirstOrDefault_NoPredicate_NonEmpty) {
    std::vector<int> v{5, 6, 7};
    EXPECT_EQ(FirstOrDefault<int>(v), 5);
}

TEST(LinqTests, FirstOrDefault_NoPredicate_Empty) {
    std::vector<int> v;
    EXPECT_EQ(FirstOrDefault<int>(v), 0);
}

// --- First ---

TEST(LinqTests, First_Found) {
    std::vector<int> v{1, 2, 3};
    EXPECT_EQ(First<int>(v, [](const int& x) { return x > 1; }), 2);
}

// Regression tests for a wave-3 audit finding: First()/Min()/Max() threw std::invalid_argument
// (an unrelated std:: exception type invisible to code catching System::Exception&) instead of
// System::InvalidOperationException, which is what real .NET's Enumerable.First/Min/Max throw
// for an empty (or, for First(predicate), non-matching) sequence.
TEST(LinqTests, First_NotFound_Throws) {
    std::vector<int> v{1, 2};
    EXPECT_THROW(First<int>(v, [](const int& x) { return x > 10; }), System::InvalidOperationException);
}

TEST(LinqTests, First_NoPredicate_Empty_Throws) {
    std::vector<int> v;
    EXPECT_THROW(First<int>(v), System::InvalidOperationException);
}

// --- LastOrDefault ---

TEST(LinqTests, LastOrDefault_WithPredicate) {
    std::vector<int> v{1, 2, 3, 4};
    EXPECT_EQ(LastOrDefault<int>(v, [](const int& x) { return x % 2 == 0; }), 4);
}

TEST(LinqTests, LastOrDefault_NoPredicate) {
    std::vector<int> v{10, 20, 30};
    EXPECT_EQ(LastOrDefault<int>(v), 30);
}

TEST(LinqTests, LastOrDefault_EmptySequence) {
    std::vector<int> v;
    EXPECT_EQ(LastOrDefault<int>(v), 0);
}

// --- Any / All ---

TEST(LinqTests, Any_WithPredicate_True) {
    std::vector<int> v{1, 2, 3};
    EXPECT_TRUE(Any<int>(v, [](const int& x) { return x == 2; }));
}

TEST(LinqTests, Any_WithPredicate_False) {
    std::vector<int> v{1, 3, 5};
    EXPECT_FALSE(Any<int>(v, [](const int& x) { return x == 2; }));
}

TEST(LinqTests, Any_NoPredicate_NonEmpty) {
    EXPECT_TRUE(Any<int>({1}));
}

TEST(LinqTests, Any_NoPredicate_Empty) {
    EXPECT_FALSE(Any<int>({}));
}

TEST(LinqTests, All_True) {
    std::vector<int> v{2, 4, 6};
    EXPECT_TRUE(All<int>(v, [](const int& x) { return x % 2 == 0; }));
}

TEST(LinqTests, All_False) {
    std::vector<int> v{2, 3, 6};
    EXPECT_FALSE(All<int>(v, [](const int& x) { return x % 2 == 0; }));
}

TEST(LinqTests, All_EmptySequence_True) {
    EXPECT_TRUE(All<int>({}, [](const int& x) { return x > 0; }));
}

// --- Count ---

TEST(LinqTests, Count_WithPredicate) {
    std::vector<int> v{1, 2, 3, 4, 5};
    EXPECT_EQ(Count<int>(v, [](const int& x) { return x > 3; }), 2);
}

TEST(LinqTests, Count_NoPredicate) {
    std::vector<int> v{1, 2, 3};
    EXPECT_EQ(Count<int>(v), 3);
}

// --- ToList ---

TEST(LinqTests, ToList_ReturnsCopy) {
    std::vector<int> v{1, 2, 3};
    auto copy = ToList<int>(v);
    EXPECT_EQ(copy, v);
}

// --- Sum ---

TEST(LinqTests, Sum_Integers) {
    std::vector<int> v{1, 2, 3, 4, 5};
    EXPECT_EQ(Sum<int>(v), 15);
}

TEST(LinqTests, Sum_WithSelector) {
    std::vector<int> v{1, 2, 3};
    EXPECT_EQ((Sum<int, int>(v, [](const int& x) { return x * 2; })), 12);
}

// --- Min / Max ---

TEST(LinqTests, Min_Integers) {
    std::vector<int> v{3, 1, 4, 1, 5};
    EXPECT_EQ(Min<int>(v), 1);
}

TEST(LinqTests, Max_Integers) {
    std::vector<int> v{3, 1, 4, 1, 5};
    EXPECT_EQ(Max<int>(v), 5);
}

TEST(LinqTests, Min_EmptyThrows) {
    EXPECT_THROW(Min<int>({}), System::InvalidOperationException);
}

TEST(LinqTests, Max_EmptyThrows) {
    EXPECT_THROW(Max<int>({}), System::InvalidOperationException);
}

// --- OrderBy / OrderByDescending ---

TEST(LinqTests, OrderBy_Ascending) {
    std::vector<int> v{3, 1, 4, 1, 5, 9};
    auto r = OrderBy<int, int>(v, [](const int& x) { return x; });
    EXPECT_EQ(r[0], 1);
    EXPECT_EQ(r.back(), 9);
}

TEST(LinqTests, OrderByDescending) {
    std::vector<int> v{3, 1, 4, 1, 5};
    auto r = OrderByDescending<int, int>(v, [](const int& x) { return x; });
    EXPECT_EQ(r[0], 5);
    EXPECT_EQ(r.back(), 1);
}

TEST(LinqTests, OrderBy_ByStringLength) {
    std::vector<std::string> v{"banana", "fig", "apple"};
    auto r = OrderBy<std::string, size_t>(v, [](const std::string& s) { return s.size(); });
    EXPECT_EQ(r[0], "fig");
    EXPECT_EQ(r.back(), "banana");
}

// Regression tests for ticket 332: real .NET's Enumerable.OrderBy/OrderByDescending are
// documented as stable sorts -- "if the keys of two elements are equal, the order of the
// elements is preserved" -- but the port previously used std::sort, which gives no such
// guarantee. Uses a pair's second element as a distinguishing tiebreaker: all pairs share the
// same sort key (first element), so only a stable sort preserves their original relative order
// in the second element.
TEST(LinqTests, OrderBy_EqualKeys_PreservesOriginalRelativeOrder) {
    std::vector<std::pair<int, int>> v{{1, 0}, {1, 1}, {1, 2}, {1, 3}, {1, 4}};
    auto r = OrderBy<std::pair<int, int>, int>(v, [](const std::pair<int, int>& p) { return p.first; });
    for (size_t i = 0; i < r.size(); ++i)
        EXPECT_EQ(r[i].second, static_cast<int>(i));
}
TEST(LinqTests, OrderByDescending_EqualKeys_PreservesOriginalRelativeOrder) {
    std::vector<std::pair<int, int>> v{{1, 0}, {1, 1}, {1, 2}, {1, 3}, {1, 4}};
    auto r = OrderByDescending<std::pair<int, int>, int>(v, [](const std::pair<int, int>& p) { return p.first; });
    for (size_t i = 0; i < r.size(); ++i)
        EXPECT_EQ(r[i].second, static_cast<int>(i));
}

// --- Distinct ---

TEST(LinqTests, Distinct_RemovesDuplicates) {
    std::vector<int> v{1, 2, 2, 3, 1};
    auto r = Distinct<int>(v);
    ASSERT_EQ(r.size(), 3u);
    EXPECT_EQ(r[0], 1);
    EXPECT_EQ(r[1], 2);
    EXPECT_EQ(r[2], 3);
}

// --- Reverse ---

TEST(LinqTests, Reverse_ReversesOrder) {
    std::vector<int> v{1, 2, 3};
    auto r = Reverse<int>(v);
    EXPECT_EQ(r[0], 3);
    EXPECT_EQ(r[2], 1);
}

// --- Skip / Take ---

TEST(LinqTests, Skip_RemovesFirst) {
    std::vector<int> v{1, 2, 3, 4, 5};
    auto r = Skip<int>(v, 2);
    ASSERT_EQ(r.size(), 3u);
    EXPECT_EQ(r[0], 3);
}

TEST(LinqTests, Skip_MoreThanSize_Empty) {
    EXPECT_TRUE(Skip<int>({1, 2}, 5).empty());
}

TEST(LinqTests, Take_TakesFirst) {
    std::vector<int> v{1, 2, 3, 4, 5};
    auto r = Take<int>(v, 3);
    ASSERT_EQ(r.size(), 3u);
    EXPECT_EQ(r[2], 3);
}

TEST(LinqTests, Take_Zero_Empty) {
    EXPECT_TRUE(Take<int>({1, 2}, 0).empty());
}

// --- Concat ---

TEST(LinqTests, Concat_TwoSequences) {
    std::vector<int> a{1, 2};
    std::vector<int> b{3, 4};
    auto r = Concat<int>(a, b);
    ASSERT_EQ(r.size(), 4u);
    EXPECT_EQ(r[3], 4);
}

// --- Contains ---

TEST(LinqTests, Contains_Found) {
    std::vector<int> v{1, 2, 3};
    EXPECT_TRUE(Contains<int>(v, 2));
}

TEST(LinqTests, Contains_NotFound) {
    EXPECT_FALSE(Contains<int>({1, 2, 3}, 9));
}

// ---------------------------------------------------------------------------
// Empty std::function callbacks (#1870 / SR-AUD-134 / CCF-011)
//
// Every callback overload stored its callable as a std::function and never validated it,
// so failure was data-dependent: an empty sequence returned an ordinary result,
// OrderBy/OrderByDescending did so for a one-element sequence too (std::stable_sort never
// compares below two), First(empty, {}) reported InvalidOperationException instead of the
// argument error, and anything larger reached the native std::bad_function_call -- which
// does not derive from System::Exception. .NET rejects a null delegate at the argument
// boundary before examining the sequence. See docs/EmptyCallableBoundaryPlan.md.
// ---------------------------------------------------------------------------

namespace {
    using LinqPred = std::function<bool(const int&)>;
    using LinqSel  = std::function<int(const int&)>;

    const std::vector<int> kLinqEmpty{};
    const std::vector<int> kLinqOne{7};
    const std::vector<int> kLinqTwo{7, 3};

    void expectLinqParamName(const std::function<void()>& call, const char* param) {
        try {
            call();
            FAIL() << "expected ArgumentNullException naming '" << param << "'";
        } catch (const System::ArgumentNullException& e) {
            EXPECT_EQ(std::string(e.what()),
                      std::string("Value cannot be null. (Parameter '") + param + "')");
        }
    }
} // namespace

TEST(LinqTests, EmptyPredicate_EveryOverload_ThrowsRegardlessOfLength) {
    for (const std::vector<int>* v : {&kLinqEmpty, &kLinqOne, &kLinqTwo}) {
        EXPECT_THROW((void)Where<int>(*v, LinqPred{}), System::ArgumentNullException);
        EXPECT_THROW((void)FirstOrDefault<int>(*v, LinqPred{}), System::ArgumentNullException);
        EXPECT_THROW((void)First<int>(*v, LinqPred{}), System::ArgumentNullException);
        EXPECT_THROW((void)LastOrDefault<int>(*v, LinqPred{}), System::ArgumentNullException);
        EXPECT_THROW((void)Any<int>(*v, LinqPred{}), System::ArgumentNullException);
        EXPECT_THROW((void)All<int>(*v, LinqPred{}), System::ArgumentNullException);
        EXPECT_THROW((void)Count<int>(*v, LinqPred{}), System::ArgumentNullException);
    }
}

TEST(LinqTests, EmptySelector_EveryOverload_ThrowsRegardlessOfLength) {
    for (const std::vector<int>* v : {&kLinqEmpty, &kLinqOne, &kLinqTwo}) {
        EXPECT_THROW((void)(Select<int, int>(*v, LinqSel{})), System::ArgumentNullException);
        EXPECT_THROW((void)(Sum<int, int>(*v, LinqSel{})), System::ArgumentNullException);
    }
}

TEST(LinqTests, EmptyKeySelector_OrderBy_ThrowsBelowTwoElementsToo) {
    // std::stable_sort never invokes the comparator below two elements, so these used to
    // succeed silently with an empty key selector.
    for (const std::vector<int>* v : {&kLinqEmpty, &kLinqOne, &kLinqTwo}) {
        EXPECT_THROW((void)(OrderBy<int, int>(*v, LinqSel{})), System::ArgumentNullException);
        EXPECT_THROW((void)(OrderByDescending<int, int>(*v, LinqSel{})),
                     System::ArgumentNullException);
    }
}

TEST(LinqTests, EmptyCallback_ParamNamesMatchDotNet) {
    expectLinqParamName([&] { (void)Where<int>(kLinqOne, LinqPred{}); }, "predicate");
    expectLinqParamName([&] { (void)Any<int>(kLinqOne, LinqPred{}); }, "predicate");
    expectLinqParamName([&] { (void)Count<int>(kLinqOne, LinqPred{}); }, "predicate");
    expectLinqParamName([&] { (void)Select<int, int>(kLinqOne, LinqSel{}); }, "selector");
    expectLinqParamName([&] { (void)Sum<int, int>(kLinqOne, LinqSel{}); }, "selector");
    expectLinqParamName([&] { (void)OrderBy<int, int>(kLinqOne, LinqSel{}); }, "keySelector");
    expectLinqParamName([&] { (void)OrderByDescending<int, int>(kLinqOne, LinqSel{}); },
                        "keySelector");
}

TEST(LinqTests, EmptyPredicate_First_ArgumentErrorPrecedesSequenceError) {
    // Before the repair the empty sequence reported InvalidOperationException, hiding the
    // invalid argument. .NET validates predicate first (First.cs:110).
    EXPECT_THROW((void)First<int>(kLinqEmpty, LinqPred{}), System::ArgumentNullException);
    // A real predicate still gets the sequence error.
    EXPECT_THROW((void)First<int>(kLinqEmpty, LinqPred{[](const int&) { return true; }}),
                 System::InvalidOperationException);
}

TEST(LinqTests, EmptyPredicate_All_NoLongerReturnsVacuousTrue) {
    EXPECT_THROW((void)All<int>(kLinqEmpty, LinqPred{}), System::ArgumentNullException);
    EXPECT_TRUE(All<int>(kLinqEmpty, LinqPred{[](const int&) { return false; }}));
}

TEST(LinqTests, EmptyCallback_IsCatchableAsSystemException) {
    bool caught = false;
    try {
        (void)Any<int>(kLinqOne, LinqPred{});
    } catch (const System::Exception&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
}

TEST(LinqTests, NonEmptyCallbacks_StillBehaveAsBefore) {
    const std::vector<int> v{5, 1, 4, 2};
    LinqPred even = [](const int& x) { return x % 2 == 0; };
    LinqSel  dbl  = [](const int& x) { return x * 2; };

    EXPECT_EQ(Where<int>(v, even), (std::vector<int>{4, 2}));
    EXPECT_EQ((Select<int, int>(v, dbl)), (std::vector<int>{10, 2, 8, 4}));
    EXPECT_EQ(FirstOrDefault<int>(v, even), 4);
    EXPECT_EQ(First<int>(v, even), 4);
    EXPECT_EQ(LastOrDefault<int>(v, even), 2);
    EXPECT_TRUE(Any<int>(v, even));
    EXPECT_FALSE(All<int>(v, even));
    EXPECT_EQ(Count<int>(v, even), 2);
    EXPECT_EQ((Sum<int, int>(v, dbl)), 24);
    EXPECT_EQ((OrderBy<int, int>(v, dbl)), (std::vector<int>{1, 2, 4, 5}));
    EXPECT_EQ((OrderByDescending<int, int>(v, dbl)), (std::vector<int>{5, 4, 2, 1}));
}
