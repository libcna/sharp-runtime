// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <bit>
#include <string>
#include <type_traits>

#include "System/RuntimeArgumentHandle.hpp"
#include "System/ArgIterator.hpp"

namespace {

    // SR-AUD-112 (ticket #2275). ArgIterator declares two constructors, both of which
    // throw, and declaring any constructor suppresses the implicit default one -- so the
    // type cannot be constructed at all. These tests used to fabricate an object instead:
    // an alignas(ArgIterator) char buffer, reinterpret_cast to ArgIterator*, and member
    // calls through that pointer. No object's lifetime ever began there, so every one of
    // those calls was undefined behaviour ([basic.life]) that happened to work only
    // because the methods are empty.
    //
    // std::bit_cast is the one route that needs no constructor and still produces an
    // OBJECT: it is constrained only on trivial copyability and equal size, and returns an
    // object of the destination type. ArgIterator is empty (sizeof 1), so there is no
    // value representation to get wrong. The properties this relies on are pinned by
    // ArgIteratorContractTests below, so a change that invalidates the mechanism fails
    // loudly rather than tempting the next author back into raw storage.
    System::ArgIterator MakeArgIterator() {
        return std::bit_cast<System::ArgIterator>(static_cast<unsigned char>(0));
    }

} // namespace

// ===========================================================================
// RuntimeArgumentHandle
// ===========================================================================

TEST(RuntimeArgumentHandleTests, DefaultConstruct_DoesNotThrow) {
    EXPECT_NO_THROW(System::RuntimeArgumentHandle{});
}

TEST(RuntimeArgumentHandleTests, IsDefaultConstructible) {
    System::RuntimeArgumentHandle h;
    (void)h;
    SUCCEED();
}

TEST(RuntimeArgumentHandleTests, IsCopyConstructible) {
    System::RuntimeArgumentHandle h1;
    System::RuntimeArgumentHandle h2 = h1;
    (void)h2;
    SUCCEED();
}

// ===========================================================================
// ArgIterator
// ===========================================================================

TEST(ArgIteratorTests, Constructor_Throws) {
    System::RuntimeArgumentHandle h;
    EXPECT_THROW(System::ArgIterator it(h), System::NotSupportedException);
}

TEST(ArgIteratorTests, Constructor_WithPtr_Throws) {
    System::RuntimeArgumentHandle h;
    EXPECT_THROW(System::ArgIterator it(h, nullptr), System::NotSupportedException);
}

TEST(ArgIteratorTests, End_DoesNotThrow) {
    auto it = MakeArgIterator();
    EXPECT_NO_THROW(it.End());
}

TEST(ArgIteratorTests, GetHashCode_ReturnsZero) {
    auto it = MakeArgIterator();
    EXPECT_EQ(it.GetHashCode(), 0);
}

TEST(ArgIteratorTests, Equals_ReturnsFalse) {
    auto a = MakeArgIterator();
    auto b = MakeArgIterator();
    EXPECT_FALSE(a.Equals(b));
}

TEST(ArgIteratorTests, GetNextArg_Throws) {
    auto it = MakeArgIterator();
    EXPECT_THROW(it.GetNextArg(), System::NotSupportedException);
}

TEST(ArgIteratorTests, GetNextArgType_Throws) {
    auto it = MakeArgIterator();
    EXPECT_THROW(it.GetNextArgType(), System::NotSupportedException);
}

TEST(ArgIteratorTests, GetRemainingCount_Throws) {
    auto it = MakeArgIterator();
    EXPECT_THROW(it.GetRemainingCount(), System::NotSupportedException);
}

// ===========================================================================
// ArgIterator contract — the facts the fixture above depends on (SR-AUD-112)
// ===========================================================================

// This is why the fixture could not simply write `System::ArgIterator it;`, and
// why raw storage was reached for in the first place.
TEST(ArgIteratorContractTests, IsNotDefaultConstructible) {
    EXPECT_FALSE(std::is_default_constructible_v<System::ArgIterator>);
}

// The two preconditions std::bit_cast imposes. If either stops holding, this test
// fails before the fixture can silently go back to fabricating an object.
TEST(ArgIteratorContractTests, IsEmptyAndTriviallyCopyable) {
    EXPECT_TRUE(std::is_trivially_copyable_v<System::ArgIterator>);
    EXPECT_TRUE(std::is_empty_v<System::ArgIterator>);
    EXPECT_EQ(sizeof(System::ArgIterator), 1u);
}

TEST(ArgIteratorContractTests, HandleConstructor_MessageNamesTheUnsupportedFeature) {
    System::RuntimeArgumentHandle h;
    try {
        System::ArgIterator it(h);
        FAIL() << "expected NotSupportedException";
    } catch (const System::NotSupportedException& ex) {
        EXPECT_EQ(std::string(ex.getMessageProperty()),
                  "ArgIterator requires CLR __arglist support and is not available in sharp-runtime.");
    }
}

TEST(ArgIteratorContractTests, GetRemainingCount_MessageNamesTheMember) {
    auto it = MakeArgIterator();
    try {
        (void)it.GetRemainingCount();
        FAIL() << "expected NotSupportedException";
    } catch (const System::NotSupportedException& ex) {
        EXPECT_EQ(std::string(ex.getMessageProperty()),
                  "ArgIterator.GetRemainingCount requires CLR __arglist support and is not "
                  "available in sharp-runtime.");
    }
}
