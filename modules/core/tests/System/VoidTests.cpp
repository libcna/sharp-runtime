// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <type_traits>
#include <vector>
#include "System/Void.hpp"

using System::Void;

TEST(VoidTest, DefaultConstruct) {
    Void v;
    (void)v;
}

TEST(VoidTest, ToStringEmpty) {
    Void v;
    EXPECT_EQ(v.ToString(), "");
}

TEST(VoidTest, EqualityAlwaysTrue) {
    Void a, b;
    EXPECT_TRUE(a == b);
}

TEST(VoidTest, InequalityAlwaysFalse) {
    Void a, b;
    EXPECT_FALSE(a != b);
}

TEST(VoidTest, UsableAsTemplateArg) {
    std::vector<Void> vec(3);
    EXPECT_EQ(vec.size(), 3u);
}

// The one property this port genuinely holds in common with .NET's
// `System.Void`: the struct declares no field. Everything else the type offers
// -- construction as an ordinary object, equality, `ToString()` -- is a C++
// extension of this project's own making, documented as such in the header,
// because C# rejects every analogous use of `System.Void` (CS0673, and CS0143
// for construction). This pins the shape, which is what a future change to the
// extensions must not silently take away; `sizeof == 1` alone would not, since
// a one-byte data member keeps that equality while destroying emptiness.
// SR-AUD-136, ticket #2286.
TEST(VoidTest, IsAnEmptyFieldlessTagType) {
    EXPECT_TRUE(std::is_empty_v<Void>);
    EXPECT_FALSE(std::is_polymorphic_v<Void>);
    EXPECT_TRUE(std::is_standard_layout_v<Void>);
    EXPECT_TRUE(std::is_trivially_default_constructible_v<Void>);
    EXPECT_TRUE(std::is_trivially_copyable_v<Void>);
    EXPECT_TRUE(std::is_trivially_destructible_v<Void>);
    EXPECT_EQ(sizeof(Void), 1u);
}
