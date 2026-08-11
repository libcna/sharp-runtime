// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2264 (finding SR-AUD-111) — public-header self-sufficiency regression.
//
// The include below MUST stay the first include in this translation unit, ahead
// of <gtest/gtest.h> and ahead of every other System header. That is the whole
// point of the file: before #2264, ModuleHandle.hpp forward-declared
// RuntimeTypeHandle and then defined ResolveTypeHandle with that incomplete type
// as its return type, which [dcl.fct]/12 forbids, so a translation unit whose
// first System include was this header failed to compile before it could call
// any API. Every existing suite hid the fault by including RuntimeTypeHandle.hpp
// first (Batch15TypesTests.cpp:8 before :11), which completes the cycle from the
// other side.
//
// If a future change reintroduces the defect, this file stops compiling. Do not
// "fix" a build break here by adding an include above this line or by reordering
// it — that would restore the masking the finding is about. Fix the header.
#include "System/ModuleHandle.hpp"

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

namespace {

// The contract the finding is about, stated as a compile-time assertion rather
// than left implicit in whether the file happens to build: including
// ModuleHandle.hpp alone must leave BOTH handle types complete, because
// ModuleHandle::ResolveTypeHandle returns a RuntimeTypeHandle by value.
static_assert(sizeof(System::ModuleHandle) > 0,
              "ModuleHandle must be complete after including only ModuleHandle.hpp");
static_assert(sizeof(System::RuntimeTypeHandle) > 0,
              "RuntimeTypeHandle must be complete after including only ModuleHandle.hpp");
static_assert(std::is_same_v<decltype(std::declval<const System::ModuleHandle&>()
                                          .ResolveTypeHandle(0)),
                             System::RuntimeTypeHandle>,
              "ResolveTypeHandle must still return RuntimeTypeHandle by value");

} // namespace

// ===========================================================================
// ModuleHandle — reachable from a standalone include of its own header
// ===========================================================================

TEST(ModuleHandleStandaloneIncludeTests, EmptyHandleIsUsable) {
    EXPECT_EQ(System::ModuleHandle::EmptyHandle.getMDStreamVersionProperty(), 0);
    EXPECT_EQ(System::ModuleHandle::EmptyHandle.GetHashCode(), 0);
}

TEST(ModuleHandleStandaloneIncludeTests, DefaultConstructedEqualsEmptyHandle) {
    const System::ModuleHandle handle;
    EXPECT_TRUE(handle.Equals(System::ModuleHandle::EmptyHandle));
    EXPECT_TRUE(handle == System::ModuleHandle::EmptyHandle);
    EXPECT_FALSE(handle != System::ModuleHandle::EmptyHandle);
}

TEST(ModuleHandleStandaloneIncludeTests, ResolveTypeHandleStillThrowsNotSupported) {
    const System::ModuleHandle handle;
    EXPECT_THROW((void)handle.ResolveTypeHandle(42), System::NotSupportedException);

    // ResolveTypeHandle is [[noreturn]], so a FAIL() after the call would be
    // provably dead code; record the arrival explicitly instead.
    bool caught = false;
    try {
        (void)handle.ResolveTypeHandle(42);
    } catch (const System::NotSupportedException& exception) {
        caught = true;
        // The deferred definition must not have altered the stub's message.
        EXPECT_STREQ(exception.what(), "ModuleHandle.ResolveTypeHandle is not supported.");
    }
    EXPECT_TRUE(caught);
}

// ===========================================================================
// RuntimeTypeHandle — the other side of the cycle, still reachable from here
// ===========================================================================

TEST(ModuleHandleStandaloneIncludeTests, RuntimeTypeHandleRoundTripsThroughThisInclude) {
    const System::RuntimeTypeHandle handle = System::RuntimeTypeHandle::FromIntPtr(7);
    EXPECT_EQ(handle.getValueProperty(), 7);
    EXPECT_EQ(System::RuntimeTypeHandle::ToIntPtr(handle), 7);
}

TEST(ModuleHandleStandaloneIncludeTests, GetModuleHandleResolvesFromThisIncludeOrder) {
    // The mirrored deferred definition in RuntimeTypeHandle.hpp has to be usable
    // when ModuleHandle.hpp is the header that pulled it in, not only the other
    // way round.
    const System::RuntimeTypeHandle handle;
    const System::ModuleHandle module = handle.GetModuleHandle();
    EXPECT_TRUE(module.Equals(System::ModuleHandle::EmptyHandle));
}

// ===========================================================================
// Layout — the repair moved a member function body out of the class, which must
// not have disturbed the type it was declared in.
// ===========================================================================

TEST(ModuleHandleStandaloneIncludeTests, LayoutIsUnchangedByTheDeferredDefinition) {
    EXPECT_EQ(sizeof(System::ModuleHandle), 1u);
    EXPECT_EQ(alignof(System::ModuleHandle), 1u);
    EXPECT_TRUE(std::is_trivially_copyable_v<System::ModuleHandle>);
    EXPECT_TRUE(std::is_standard_layout_v<System::ModuleHandle>);
    EXPECT_TRUE(std::is_aggregate_v<System::ModuleHandle>);
}
