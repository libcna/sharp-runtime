// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/LoaderOptimization.hpp"
#include "System/LoaderOptimizationAttribute.hpp"
#include "System/Attribute.hpp"

using System::LoaderOptimization;
using System::LoaderOptimizationAttribute;

// #2289 deprecated LoaderOptimization::DomainMask and ::DisallowBindings, transcribing .NET's
// [Obsolete] (LoaderOptimization.cs:8,10). Every use below is inside a scoped suppression, and
// THE SUPPRESSION IS THE EVIDENCE: it is REQUIRED, and deleting any of them fails this build with
// "error: ... is deprecated ... [-Werror=deprecated-declarations]" -- which is exactly the
// diagnostic SR-AUD-117 asked for, demonstrated rather than asserted.
//
// The VALUES must still be pinned: deprecating an enumerator must not change what it is, and a
// repair that quietly renumbered one would be far worse than the divergence it replaced.

// ---------------------------------------------------------------------------
// LoaderOptimization enum values
// ---------------------------------------------------------------------------

TEST(LoaderOptimizationTests, NotSpecified_Value_IsZero) {
    EXPECT_EQ(static_cast<int>(LoaderOptimization::NotSpecified), 0);
}

TEST(LoaderOptimizationTests, SingleDomain_Value_IsOne) {
    EXPECT_EQ(static_cast<int>(LoaderOptimization::SingleDomain), 1);
}

TEST(LoaderOptimizationTests, MultiDomain_Value_IsTwo) {
    EXPECT_EQ(static_cast<int>(LoaderOptimization::MultiDomain), 2);
}

TEST(LoaderOptimizationTests, MultiDomainHost_Value_IsThree) {
    EXPECT_EQ(static_cast<int>(LoaderOptimization::MultiDomainHost), 3);
}

TEST(LoaderOptimizationTests, DomainMask_Value_IsThree) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    EXPECT_EQ(static_cast<int>(LoaderOptimization::DomainMask), 3);
#pragma GCC diagnostic pop
}

TEST(LoaderOptimizationTests, DomainMask_EqualsMultiDomainHost) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    EXPECT_EQ(LoaderOptimization::DomainMask, LoaderOptimization::MultiDomainHost);
#pragma GCC diagnostic pop
}

TEST(LoaderOptimizationTests, DisallowBindings_Value_IsFour) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    EXPECT_EQ(static_cast<int>(LoaderOptimization::DisallowBindings), 4);
#pragma GCC diagnostic pop
}

// ---------------------------------------------------------------------------
// LoaderOptimizationAttribute — byte constructor
// ---------------------------------------------------------------------------

TEST(LoaderOptimizationAttributeTests, ByteCtor_Zero_IsNotSpecified) {
    LoaderOptimizationAttribute attr(uint8_t(0));
    EXPECT_EQ(attr.getValueProperty(), LoaderOptimization::NotSpecified);
}

TEST(LoaderOptimizationAttributeTests, ByteCtor_One_IsSingleDomain) {
    LoaderOptimizationAttribute attr(uint8_t(1));
    EXPECT_EQ(attr.getValueProperty(), LoaderOptimization::SingleDomain);
}

TEST(LoaderOptimizationAttributeTests, ByteCtor_Two_IsMultiDomain) {
    LoaderOptimizationAttribute attr(uint8_t(2));
    EXPECT_EQ(attr.getValueProperty(), LoaderOptimization::MultiDomain);
}

TEST(LoaderOptimizationAttributeTests, ByteCtor_Three_IsMultiDomainHost) {
    LoaderOptimizationAttribute attr(uint8_t(3));
    EXPECT_EQ(attr.getValueProperty(), LoaderOptimization::MultiDomainHost);
}

TEST(LoaderOptimizationAttributeTests, ByteCtor_Four_IsDisallowBindings) {
    LoaderOptimizationAttribute attr(uint8_t(4));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    EXPECT_EQ(attr.getValueProperty(), LoaderOptimization::DisallowBindings);
#pragma GCC diagnostic pop
}

// ---------------------------------------------------------------------------
// LoaderOptimizationAttribute — LoaderOptimization constructor
// ---------------------------------------------------------------------------

TEST(LoaderOptimizationAttributeTests, EnumCtor_NotSpecified) {
    LoaderOptimizationAttribute attr(LoaderOptimization::NotSpecified);
    EXPECT_EQ(attr.getValueProperty(), LoaderOptimization::NotSpecified);
}

TEST(LoaderOptimizationAttributeTests, EnumCtor_MultiDomain) {
    LoaderOptimizationAttribute attr(LoaderOptimization::MultiDomain);
    EXPECT_EQ(attr.getValueProperty(), LoaderOptimization::MultiDomain);
}

TEST(LoaderOptimizationAttributeTests, EnumCtor_DisallowBindings) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    LoaderOptimizationAttribute attr(LoaderOptimization::DisallowBindings);
    EXPECT_EQ(attr.getValueProperty(), LoaderOptimization::DisallowBindings);
#pragma GCC diagnostic pop
}

// ---------------------------------------------------------------------------
// Inheritance
// ---------------------------------------------------------------------------

TEST(LoaderOptimizationAttributeTests, IsA_Attribute_New) {
    LoaderOptimizationAttribute attr(LoaderOptimization::SingleDomain);
    EXPECT_NE(dynamic_cast<System::Attribute*>(&attr), nullptr);
}
