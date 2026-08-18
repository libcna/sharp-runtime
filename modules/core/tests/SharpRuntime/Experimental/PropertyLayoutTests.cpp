// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2246 (SR-AUD-179 residual). #2244 established that Property<T>'s `cachedValue` member
// was read by nothing and written by nothing; this suite is what #2246's removal of it leaves
// behind, and it pins two separate claims that a single `sizeof` number would blur together.
#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <type_traits>

#include "SharpRuntime/Experimental/Property.hpp"
#include "System/ArgumentNullException.hpp"

using SharpRuntime::Experimental::Property;

namespace {

/// A field-for-field shadow of the class, so the pin is a RELATIONSHIP rather than a number
/// re-guessed from a compiler. `sizeof(Property<T>)` is two std::functions and nothing else.
template <typename T>
struct PropertyLayoutProbe {
    std::function<T()>            getter;
    std::function<void(const T&)> setter;
};

/// Not default-constructible, and deliberately so: before #2246 this type could not be used with
/// Property<T> at all, because the vestigial member had to be default-initialised.
struct NoDefaultCtor {
    explicit NoDefaultCtor(int v) : value(v) {}
    int value;
};

}  // namespace

TEST(PropertyLayoutTests, Fix2246_TheVestigialMemberIsGoneAndTheLayoutIsPinned) {
    // The relationship, not the number. Measured before #2246: sizeof(Property<int>) was 72 and
    // sizeof(Property<std::string>) was 96, because each carried a T. Now BOTH are exactly two
    // std::functions -- which is also why the two instantiations agree with each other, where
    // before they could not.
    static_assert(sizeof(Property<int>) == sizeof(PropertyLayoutProbe<int>),
                  "#2246: Property<T> is two std::functions and nothing else");
    static_assert(sizeof(Property<std::string>) == sizeof(PropertyLayoutProbe<std::string>),
                  "#2246: and that holds for a non-trivial T too");
    static_assert(alignof(Property<int>) == alignof(PropertyLayoutProbe<int>),
                  "#2246: alignment did not move");

    EXPECT_EQ(sizeof(Property<int>), sizeof(PropertyLayoutProbe<int>));
    EXPECT_EQ(sizeof(Property<std::string>), sizeof(PropertyLayoutProbe<std::string>));
    // The size no longer depends on T at all, which is the observable shape of the removal.
    EXPECT_EQ(sizeof(Property<int>), sizeof(Property<std::string>));
}

TEST(PropertyLayoutTests, Fix2246_TDoesNotHaveToBeDefaultConstructible) {
    // THE HALF A sizeof PIN CANNOT EXPRESS. The removed member imposed a requirement this class
    // never used: every constructor default-initialised it, so a T without a default constructor
    // could not be wrapped at all. This instantiation did not compile before #2246.
    static_assert(!std::is_default_constructible_v<NoDefaultCtor>,
                  "the probe type must stay non-default-constructible for this test to mean anything");

    NoDefaultCtor storage{7};
    Property<NoDefaultCtor> prop([&storage]() { return storage; },
                                 [&storage](const NoDefaultCtor& v) { storage = v; });
    EXPECT_EQ(7, static_cast<NoDefaultCtor>(prop).value);
    prop = NoDefaultCtor{42};
    EXPECT_EQ(42, storage.value);
}

TEST(PropertyLayoutTests, Fix2246_TheTwoSurvivingMembersStillWork) {
    // Removing a member must not remove behaviour. Both remaining members are used, and #2247's
    // empty-getter check is untouched.
    int backing = 1;
    Property<int> readWrite([&backing] { return backing; }, [&backing](int v) { backing = v; });
    EXPECT_EQ(1, static_cast<int>(readWrite));
    readWrite = 5;
    EXPECT_EQ(5, backing);

    Property<int> readOnly([&backing] { return backing; });
    EXPECT_EQ(5, static_cast<int>(readOnly));

    EXPECT_THROW((Property<int>{std::function<int()>{}}), System::ArgumentNullException);
}
