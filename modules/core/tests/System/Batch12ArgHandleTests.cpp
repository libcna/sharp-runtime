// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/PlatformNotSupportedException.hpp"
#include "System/RuntimeTypeHandle.hpp"

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

// #2276 SETTLED THE OPEN QUESTION, AND THE REFERENCE ANSWERED IT.
//
// The question was whether ArgIterator's unreachable instance members should become reachable,
// become static, or stay as they are. They STAY INSTANCE MEMBERS, because .NET's are, and making
// them static would diverge from the very shape this stub exists to present.
//
// What the reference settled is that EVERY member of .NET's portable ArgIterator throws
// PlatformNotSupportedException (`ArgIterator.cs:10-58`) -- constructors, End, Equals,
// GetHashCode and the rest alike. Three members here returned quietly instead: End() was a no-op,
// Equals() returned false and GetHashCode() returned 0. A caller who reached one of those
// received a PLAUSIBLE ANSWER where .NET reports an unsupported platform, which is the worse of
// the two failures.

TEST(ArgIteratorTests, Fix2276_BothConstructorsThrowPlatformNotSupported) {
    System::RuntimeArgumentHandle h;
    EXPECT_THROW(System::ArgIterator it(h), System::PlatformNotSupportedException);
    EXPECT_THROW(System::ArgIterator it(h, nullptr), System::PlatformNotSupportedException);
}

TEST(ArgIteratorTests, Fix2276_EveryMemberThrowsIncludingTheThreeThatUsedToAnswer) {
    auto it = MakeArgIterator();
    auto other = MakeArgIterator();

    // The three that changed. Each used to return quietly.
    EXPECT_THROW(it.End(), System::PlatformNotSupportedException);
    EXPECT_THROW((void)it.GetHashCode(), System::PlatformNotSupportedException);
    EXPECT_THROW((void)it.Equals(other), System::PlatformNotSupportedException);

    // The three that already threw, now with .NET's exception type rather than the base.
    EXPECT_THROW((void)it.GetNextArg(), System::PlatformNotSupportedException);
    EXPECT_THROW((void)it.GetNextArgType(), System::PlatformNotSupportedException);
    EXPECT_THROW((void)it.GetRemainingCount(), System::PlatformNotSupportedException);

    // The overload #2276 ADDED, which .NET has and this port did not.
    EXPECT_THROW((void)it.GetNextArg(System::RuntimeTypeHandle{}),
                 System::PlatformNotSupportedException);
}

TEST(ArgIteratorTests, Fix2276_TheExceptionTypeIsPlatformNotSupportedNotItsBase) {
    // PlatformNotSupportedException derives from NotSupportedException, so a test that only
    // caught the base would pass either way -- which is exactly how the old type survived. This
    // row asserts the derived type is what arrives.
    auto it = MakeArgIterator();
    try {
        (void)it.GetRemainingCount();
        ADD_FAILURE() << "expected PlatformNotSupportedException";
    } catch (const System::PlatformNotSupportedException& e) {
        EXPECT_STREQ(e.what(), "ArgIterator is not supported on this platform.");
    }
    static_assert(std::is_base_of_v<System::NotSupportedException,
                                    System::PlatformNotSupportedException>,
                  "if this ever stops holding, the catch above is not the narrowing it claims");
}

TEST(ArgIteratorTests, Fix2276_GetNextArgTypeReturnsARuntimeTypeHandle) {
    // .NET's returns RuntimeTypeHandle (`ArgIterator.cs:50-53`); this port returned
    // TypedReference. The body throws either way, so ONLY a type assertion can catch this.
    static_assert(std::is_same_v<decltype(std::declval<System::ArgIterator&>().GetNextArgType()),
                                 System::RuntimeTypeHandle>,
                  "#2276: GetNextArgType returns a RuntimeTypeHandle, as .NET's does");
    static_assert(std::is_same_v<decltype(std::declval<System::ArgIterator&>().GetNextArg()),
                                 System::TypedReference>,
                  "...while GetNextArg still returns a TypedReference, also as .NET's does");
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

TEST(ArgIteratorContractTests, Fix2276_EveryDoorCarriesDotNetsOwnSentence) {
    // These two rows used to pin this port's INVENTED messages -- "ArgIterator requires CLR
    // __arglist support and is not available in sharp-runtime", and a per-member variant naming
    // GetRemainingCount. .NET uses ONE sentence for every door
    // (`Strings.resx:3305-3307`, SR.PlatformNotSupported_ArgIterator), so #2276 transcribes that
    // instead. A per-member message reads more helpfully and is not what the reference says.
    System::RuntimeArgumentHandle h;
    const std::string expected = "ArgIterator is not supported on this platform.";
    try {
        System::ArgIterator it(h);
        FAIL() << "expected PlatformNotSupportedException";
    } catch (const System::PlatformNotSupportedException& ex) {
        EXPECT_EQ(std::string(ex.getMessageProperty()), expected);
    }

    auto it = MakeArgIterator();
    try {
        (void)it.GetRemainingCount();
        FAIL() << "expected PlatformNotSupportedException";
    } catch (const System::PlatformNotSupportedException& ex) {
        EXPECT_EQ(std::string(ex.getMessageProperty()), expected)
            << "one sentence for every door, as .NET has -- not a per-member variant";
    }
}
