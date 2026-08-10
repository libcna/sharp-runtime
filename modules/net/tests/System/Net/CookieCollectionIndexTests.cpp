// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2041 -- SR-AUD-307, cause N-B of docs/SystemNetNamespaceReviewPlan.md.
//
// Both CookieCollection indexers used to hand a public signed index straight to
// std::vector::operator[]. Measured before the repair
// (build-probe/2041_probe1_before_after.log), the two failure shapes were NOT the same:
//
//   * WITHOUT a sanitizer, collection[-1] on a one-element collection RETURNED NORMALLY with
//     a garbage Cookie whose Name printed as "(null)" -- the finding's own named case is the
//     one that silently succeeds (plan section 4.3) -- while collection[Count] died with
//     SIGSEGV;
//   * WITH ASan, nine of the eleven probed indexes reported a heap-buffer-overflow or a SEGV,
//     and after the repair ZERO did;
//   * WITH UBSan, the two empty-collection cases reported null-pointer reference binding and
//     null-pointer arithmetic before, and none after.
//
// So a regression test must cover BOTH directions, BOTH overloads and the empty collection --
// pinning only the crashing case would leave the silently-succeeding one unguarded.
#include <gtest/gtest.h>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Net/Cookie.hpp"
#include "System/Net/CookieCollection.hpp"

using System::ArgumentOutOfRangeException;
using System::Net::Cookie;
using System::Net::CookieCollection;
using SharpRuntime::intcs;

namespace {

    CookieCollection makeCollection(intcs count) {
        CookieCollection collection;
        for (intcs i = 0; i < count; ++i)
            collection.Add(Cookie("n" + std::to_string(i), "v" + std::to_string(i)));
        return collection;
    }

    // Asserts the exact contract: type, paramName and message text, on one overload.
    void expectRejected(const CookieCollection& collection, intcs index) {
        try {
            const Cookie& cookie = collection[index];
            ADD_FAILURE() << "const operator[](" << index << ") returned instead of throwing; Name=\""
                          << cookie.getNameProperty() << "\"";
        } catch (const ArgumentOutOfRangeException& ex) {
            EXPECT_EQ(ex.getParamNameProperty(), "index");
            EXPECT_NE(std::string(ex.what()).find(
                          "Index was out of range. Must be non-negative and less than the size of the collection."),
                      std::string::npos)
                << ex.what();
        }
    }

    void expectRejectedMutable(CookieCollection& collection, intcs index) {
        try {
            Cookie& cookie = collection[index];
            ADD_FAILURE() << "mutable operator[](" << index << ") returned instead of throwing; Name=\""
                          << cookie.getNameProperty() << "\"";
        } catch (const ArgumentOutOfRangeException& ex) {
            EXPECT_EQ(ex.getParamNameProperty(), "index");
            EXPECT_NE(std::string(ex.what()).find(
                          "Index was out of range. Must be non-negative and less than the size of the collection."),
                      std::string::npos)
                << ex.what();
        }
    }

} // namespace

// ---------------------------------------------------------------------------
// The valid range is unchanged -- the repair must narrow nothing that worked.
// ---------------------------------------------------------------------------

TEST(CookieCollectionIndexTests, ValidRange_ConstOverload_Unchanged) {
    const CookieCollection collection = makeCollection(3);
    ASSERT_EQ(collection.getCountProperty(), 3);
    EXPECT_EQ(collection[0].getNameProperty(), "n0");
    EXPECT_EQ(collection[1].getNameProperty(), "n1");
    EXPECT_EQ(collection[2].getNameProperty(), "n2");
    EXPECT_EQ(collection[2].getValueProperty(), "v2");
}

TEST(CookieCollectionIndexTests, ValidRange_MutableOverload_Unchanged) {
    CookieCollection collection = makeCollection(3);
    ASSERT_EQ(collection.getCountProperty(), 3);
    EXPECT_EQ(collection[0].getNameProperty(), "n0");
    collection[1].setValueProperty("mutated");
    EXPECT_EQ(collection[1].getValueProperty(), "mutated");
    EXPECT_EQ(collection[2].getNameProperty(), "n2");
}

TEST(CookieCollectionIndexTests, SingleElement_IndexZero_StillWorks) {
    CookieCollection collection = makeCollection(1);
    ASSERT_EQ(collection.getCountProperty(), 1);
    EXPECT_EQ(collection[0].getNameProperty(), "n0");
    EXPECT_EQ(static_cast<const CookieCollection&>(collection)[0].getNameProperty(), "n0");
}

// ---------------------------------------------------------------------------
// The finding's OWN case -- the one that silently succeeded before.
// ---------------------------------------------------------------------------

TEST(CookieCollectionIndexTests, NegativeOne_ConstOverload_Throws) {
    const CookieCollection collection = makeCollection(1);
    expectRejected(collection, -1);
}

TEST(CookieCollectionIndexTests, NegativeOne_MutableOverload_Throws) {
    CookieCollection collection = makeCollection(1);
    expectRejectedMutable(collection, -1);
}

// ---------------------------------------------------------------------------
// The case that crashed before.
// ---------------------------------------------------------------------------

TEST(CookieCollectionIndexTests, IndexEqualToCount_ConstOverload_Throws) {
    const CookieCollection collection = makeCollection(1);
    expectRejected(collection, collection.getCountProperty());
}

TEST(CookieCollectionIndexTests, IndexEqualToCount_MutableOverload_Throws) {
    CookieCollection collection = makeCollection(1);
    expectRejectedMutable(collection, collection.getCountProperty());
}

TEST(CookieCollectionIndexTests, IndexBeyondCount_BothOverloads_Throw) {
    CookieCollection collection = makeCollection(3);
    expectRejectedMutable(collection, collection.getCountProperty() + 1);
    expectRejected(static_cast<const CookieCollection&>(collection), collection.getCountProperty() + 1);
}

// ---------------------------------------------------------------------------
// The signed domain's extremes -- INTCS_MIN is the value whose size_t conversion
// produced the largest wrong subscript.
// ---------------------------------------------------------------------------

TEST(CookieCollectionIndexTests, IntcsMin_BothOverloads_Throw) {
    CookieCollection collection = makeCollection(3);
    expectRejectedMutable(collection, SharpRuntime::INTCS_MIN);
    expectRejected(static_cast<const CookieCollection&>(collection), SharpRuntime::INTCS_MIN);
}

TEST(CookieCollectionIndexTests, IntcsMax_BothOverloads_Throw) {
    CookieCollection collection = makeCollection(3);
    expectRejectedMutable(collection, SharpRuntime::INTCS_MAX);
    expectRejected(static_cast<const CookieCollection&>(collection), SharpRuntime::INTCS_MAX);
}

// ---------------------------------------------------------------------------
// The empty collection -- vector::data() is null here, so before the repair even
// index 0 was a null dereference (and UBSan said so).
// ---------------------------------------------------------------------------

TEST(CookieCollectionIndexTests, EmptyCollection_EveryIndexThrows) {
    CookieCollection collection;
    ASSERT_EQ(collection.getCountProperty(), 0);
    ASSERT_TRUE(collection.getIsEmptyProperty());
    expectRejectedMutable(collection, 0);
    expectRejectedMutable(collection, -1);
    expectRejected(static_cast<const CookieCollection&>(collection), 0);
    expectRejected(static_cast<const CookieCollection&>(collection), -1);
}

// ---------------------------------------------------------------------------
// Iteration and Count are untouched by the repair.
// ---------------------------------------------------------------------------

TEST(CookieCollectionIndexTests, Iteration_And_Count_Unaffected) {
    CookieCollection collection = makeCollection(4);
    intcs seen = 0;
    for (const Cookie& cookie : collection) {
        EXPECT_EQ(cookie.getNameProperty(), "n" + std::to_string(seen));
        ++seen;
    }
    EXPECT_EQ(seen, 4);
    EXPECT_EQ(collection.getCountProperty(), 4);
    EXPECT_FALSE(collection.getIsEmptyProperty());
}

// ---------------------------------------------------------------------------
// A rejected access must not disturb the collection.
// ---------------------------------------------------------------------------

TEST(CookieCollectionIndexTests, RejectedAccess_LeavesCollectionIntact) {
    CookieCollection collection = makeCollection(2);
    EXPECT_THROW((void)collection[5], ArgumentOutOfRangeException);
    EXPECT_THROW((void)collection[-3], ArgumentOutOfRangeException);
    ASSERT_EQ(collection.getCountProperty(), 2);
    EXPECT_EQ(collection[0].getNameProperty(), "n0");
    EXPECT_EQ(collection[1].getNameProperty(), "n1");
}
