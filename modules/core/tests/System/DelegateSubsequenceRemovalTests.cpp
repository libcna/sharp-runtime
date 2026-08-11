// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Regressions for SR-AUD-120 (ticket #2273): Delegate::Remove compared every
// single invocation-list entry against the WHOLE value object, so a multicast
// value could never match and Remove always returned the source unchanged.
// .NET's MulticastDelegate.RemoveImpl removes the last matching contiguous
// subsequence instead.
//
// See docs/CoreDelegateCompositionContractPlan.md 6.

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "System/Delegate.hpp"

using System::Delegate;

namespace {

    std::string trace;

    void A() { trace += 'A'; }
    void B() { trace += 'B'; }
    void C() { trace += 'C'; }

    std::shared_ptr<Delegate> Of(void (*fn)()) { return std::make_shared<Delegate>(fn); }

    /** Builds a multicast delegate whose entries are freshly allocated per call. */
    std::shared_ptr<Delegate> List(const std::vector<void (*)()>& fns) {
        std::vector<std::shared_ptr<Delegate>> entries;
        entries.reserve(fns.size());
        for (auto fn : fns) entries.push_back(Of(fn));
        return Delegate::Combine(entries);
    }

    /** Invokes d and returns the sequence of targets it ran. */
    std::string Fired(const std::shared_ptr<Delegate>& d) {
        trace.clear();
        if (d) d->Invoke();
        return trace;
    }

    std::size_t Size(const std::shared_ptr<Delegate>& d) {
        return d ? d->GetInvocationList().size() : 0u;
    }

} // namespace

// ---------------------------------------------------------------------------
// The finding
// ---------------------------------------------------------------------------

TEST(DelegateSubsequenceRemovalTests, MulticastValue_SubsequenceIsRemoved) {
    auto source = List({A, B, A, B});
    auto value  = List({A, B});
    auto result = Delegate::Remove(source, value);
    ASSERT_TRUE(result);
    EXPECT_EQ(Size(result), 2u);
    EXPECT_EQ(Fired(result), "AB");
}

// The last match, not the first: only the trailing pair may go.
TEST(DelegateSubsequenceRemovalTests, MulticastValue_LastMatchIsRemoved) {
    auto source = List({A, B, C, A, B});
    auto value  = List({A, B});
    auto result = Delegate::Remove(source, value);
    ASSERT_TRUE(result);
    EXPECT_EQ(Fired(result), "ABC");
}

TEST(DelegateSubsequenceRemovalTests, MulticastValue_MiddleMatchIsRemoved) {
    auto source = List({C, A, B, C});
    auto value  = List({A, B});
    auto result = Delegate::Remove(source, value);
    ASSERT_TRUE(result);
    EXPECT_EQ(Fired(result), "CC");
}

// The pointers being literally the same never helped before the repair.
TEST(DelegateSubsequenceRemovalTests, MulticastValue_SameEntryPointers_Removed) {
    auto a = Of(A);
    auto b = Of(B);
    auto source = Delegate::Combine({a, b, a, b});
    auto value  = Delegate::Combine(a, b);
    auto result = Delegate::Remove(source, value);
    ASSERT_TRUE(result);
    EXPECT_EQ(Size(result), 2u);
    EXPECT_EQ(Fired(result), "AB");
}

TEST(DelegateSubsequenceRemovalTests, MulticastValue_ThreeEntrySubsequence) {
    auto source = List({C, A, B, C});
    auto value  = List({A, B, C});
    auto result = Delegate::Remove(source, value);
    ASSERT_TRUE(result);
    EXPECT_EQ(Size(result), 1u);
    EXPECT_EQ(Fired(result), "C");
}

// ---------------------------------------------------------------------------
// Boundaries
// ---------------------------------------------------------------------------

TEST(DelegateSubsequenceRemovalTests, RemovingTheWholeList_ReturnsNull) {
    auto source = List({A, B});
    auto value  = List({A, B});
    EXPECT_FALSE(Delegate::Remove(source, value));
}

TEST(DelegateSubsequenceRemovalTests, LeavingOneEntry_ReturnsThatEntryItself) {
    auto a = Of(A);
    auto source = Delegate::Combine({a, Of(B), Of(C)});
    auto value  = List({B, C});
    auto result = Delegate::Remove(source, value);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.get(), a.get());
    EXPECT_TRUE(result->getHasSingleTargetProperty());
    EXPECT_EQ(Size(result), 1u);
}

TEST(DelegateSubsequenceRemovalTests, ValueLongerThanSource_Unchanged) {
    auto source = List({A, B});
    auto value  = List({A, B, C});
    auto result = Delegate::Remove(source, value);
    EXPECT_EQ(result.get(), source.get());
}

TEST(DelegateSubsequenceRemovalTests, NonContiguousMatch_Unchanged) {
    auto source = List({A, C, B});
    auto value  = List({A, B});
    auto result = Delegate::Remove(source, value);
    EXPECT_EQ(result.get(), source.get());
}

TEST(DelegateSubsequenceRemovalTests, ReorderedValue_Unchanged) {
    auto source = List({A, B});
    auto value  = List({B, A});
    auto result = Delegate::Remove(source, value);
    EXPECT_EQ(result.get(), source.get());
}

TEST(DelegateSubsequenceRemovalTests, SingleTargetSource_MulticastValue_Unchanged) {
    auto source = Of(A);
    auto value  = List({A, B});
    auto result = Delegate::Remove(source, value);
    EXPECT_EQ(result.get(), source.get());
}

// A closure has no comparable target, so a value built from separate lambdas
// still cannot match. The repair must not invent an equality C++ does not have.
TEST(DelegateSubsequenceRemovalTests, LambdaEntries_IndependentValue_Unchanged) {
    auto source = Delegate::Combine({std::make_shared<Delegate>([]{}),
                                     std::make_shared<Delegate>([]{})});
    auto value  = Delegate::Combine(std::make_shared<Delegate>([]{}),
                                    std::make_shared<Delegate>([]{}));
    auto result = Delegate::Remove(source, value);
    EXPECT_EQ(result.get(), source.get());
}

// ---------------------------------------------------------------------------
// RemoveAll inherits the fix, as the finding states
// ---------------------------------------------------------------------------

TEST(DelegateSubsequenceRemovalTests, RemoveAll_RepeatedSubsequence_ReturnsNull) {
    auto source = List({A, B, A, B});
    auto value  = List({A, B});
    EXPECT_FALSE(Delegate::RemoveAll(source, value));
}

TEST(DelegateSubsequenceRemovalTests, RemoveAll_LeavesNonMatchingEntries) {
    auto source = List({A, B, C, A, B});
    auto value  = List({A, B});
    auto result = Delegate::RemoveAll(source, value);
    ASSERT_TRUE(result);
    EXPECT_EQ(Fired(result), "C");
}

// ---------------------------------------------------------------------------
// The single-target removal path must be untouched
// ---------------------------------------------------------------------------

TEST(DelegateSubsequenceRemovalTests, SingleValue_LastOccurrenceStillRemoved) {
    auto source = List({A, B, A});
    auto result = Delegate::Remove(source, Of(A));
    ASSERT_TRUE(result);
    EXPECT_EQ(Size(result), 2u);
    EXPECT_EQ(Fired(result), "AB");
}

TEST(DelegateSubsequenceRemovalTests, SingleValue_NotPresent_Unchanged) {
    auto source = List({A, B});
    auto result = Delegate::Remove(source, Of(C));
    EXPECT_EQ(result.get(), source.get());
}

TEST(DelegateSubsequenceRemovalTests, NullValue_ReturnsSource) {
    auto source = List({A, B});
    EXPECT_EQ(Delegate::Remove(source, nullptr).get(), source.get());
}
