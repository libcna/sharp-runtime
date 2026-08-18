// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for System-level attribute and annotation types:
// Attribute, AttributeTargets, AttributeUsageAttribute, CLSCompliantAttribute,
// ObsoleteAttribute, FlagsAttribute, and other marker attributes.
#include <gtest/gtest.h>
#include <optional>
#include <type_traits>
#include <string>
#include "System/Attribute.hpp"
#include "System/AttributeTargets.hpp"
#include "System/AttributeUsageAttribute.hpp"
#include "System/CLSCompliantAttribute.hpp"
#include "System/ObsoleteAttribute.hpp"
#include "System/FlagsAttribute.hpp"
#include "System/ParamArrayAttribute.hpp"
#include "System/NonSerializedAttribute.hpp"
#include "System/SerializableAttribute.hpp"
#include "System/ThreadStaticAttribute.hpp"
#include "System/LoaderOptimization.hpp"
#include "System/LoaderOptimizationAttribute.hpp"

using System::Attribute;
using System::AttributeTargets;
using System::AttributeUsageAttribute;
using System::CLSCompliantAttribute;
using System::ObsoleteAttribute;

// ===========================================================================
// Attribute (base)
// ===========================================================================
//
// #2339 / SR-AUD-114. The base's constructor is PROTECTED since #2339, matching .NET's
// `protected Attribute()`. It used to be public "so that the class can be instantiated in tests",
// with a doc-comment asking callers to treat the class as logically abstract -- and these nine
// tests were the instantiations that comment existed for. They now go through a local derived
// probe, which is what a caller has to do too.
//
// WHAT IS NOT REPAIRED, AND CANNOT BE: .NET's Equals compares every instance field and its hash
// comes from the first non-array one. That is REFLECTION, permanently out of scope per CLAUDE.md,
// and a C++ base class cannot enumerate a derived class's fields at all. The identity default
// therefore stays, and is pinned below as a deviation rather than left to be rediscovered.

namespace {

/// The minimal legal way to reach the base after #2339 -- and the shape every one of the
/// repository's forty-six Attribute subclasses already has.
class ProbeAttribute : public Attribute {};

/// A second, distinct subclass, so the tests can say something about TYPE as well as identity.
class OtherProbeAttribute : public Attribute {};

}  // namespace

TEST(AttributeTests, Fix2339_TheBaseIsNoLongerDirectlyConstructible) {
    // The whole of the repair, stated as a compile-time fact. `System::Attribute a;` used to
    // compile; .NET rejects the equivalent with CS0144 and so does this port now.
    static_assert(!std::is_default_constructible_v<Attribute>,
                  "#2339: Attribute's constructor is protected, as .NET's is");
    static_assert(!std::is_copy_constructible_v<Attribute>,
                  "#2339: and the copy member went with it, so the base cannot be reached by a slice");
    static_assert(std::is_default_constructible_v<ProbeAttribute>,
                  "a derived type must still be constructible -- that is the point");
    static_assert(std::is_abstract_v<Attribute> == false,
                  "Attribute is NOT abstract: it has no pure virtual, and .NET's is abstract only "
                  "by declaration, which C++ has no counterpart for without inventing one");
}

TEST(AttributeTests, DefaultCtor_IsDefaultAttributeFalse) {
    ProbeAttribute a;
    EXPECT_FALSE(a.getIsDefaultAttributeProperty());
}

TEST(AttributeTests, Match_SameInstance_True) {
    ProbeAttribute a;
    EXPECT_TRUE(a.Match(a));
}

TEST(AttributeTests, Match_DifferentInstance_False) {
    ProbeAttribute a, b;
    EXPECT_FALSE(a.Match(b));
}

TEST(AttributeTests, Equals_SameInstance_True) {
    ProbeAttribute a;
    EXPECT_TRUE(a.Equals(a));
}

TEST(AttributeTests, Equals_DifferentInstance_False) {
    ProbeAttribute a, b;
    EXPECT_FALSE(a.Equals(b));
}

TEST(AttributeTests, GetHashCode_SameInstance_Consistent) {
    ProbeAttribute a;
    EXPECT_EQ(a.GetHashCode(), a.GetHashCode());
}

TEST(AttributeTests, GetHashCode_DifferentInstances_TypicallyDifferent) {
    ProbeAttribute a, b;
    // Identity-based hash: different addresses → different hashes (not guaranteed
    // by the contract, but true in practice for stack objects).
    // We just verify both calls succeed without throwing.
    (void)a.GetHashCode();
    (void)b.GetHashCode();
}

TEST(AttributeTests, TypeId_ReturnsAttributeType) {
    ProbeAttribute a;
    EXPECT_EQ(a.getTypeIdProperty(), typeid(ProbeAttribute))
        << "TypeId is the MOST-DERIVED type, so a probe reports itself rather than the base";
}

TEST(AttributeTests, Match_DelegatesTo_Equals) {
    // Match should return the same result as Equals for the base class.
    ProbeAttribute a, b;
    EXPECT_EQ(a.Match(a), a.Equals(a));
    EXPECT_EQ(a.Match(b), a.Equals(b));
}

TEST(AttributeTests, Decl2339_TheIdentityDefaultIsAPermanentDeviationAndReachesEverySubclass) {
    // NOT A REPAIR -- A DECLARATION, pinned so it cannot be mistaken for an oversight later.
    //
    // .NET's Attribute.Equals compares instance fields, so two independently constructed
    // CLSCompliantAttribute(true) objects are EQUAL there. Here they are not, and the measured
    // reason is that all forty-six Attribute subclasses in this repository inherit the base's
    // identity Equals without overriding it.
    const CLSCompliantAttribute first(true);
    const CLSCompliantAttribute second(true);
    EXPECT_FALSE(first.Equals(second))
        << "identity, not value -- .NET would report these EQUAL; see SR-AUD-114";
    EXPECT_TRUE(first.Equals(first));

    // The deviation is not type-blind either: two different subclasses are unequal for the same
    // identity reason, not because their types differ.
    ProbeAttribute      probe;
    OtherProbeAttribute other;
    EXPECT_FALSE(probe.Equals(other));

    // Both hooks a subclass would need are already virtual, so opting in needs no new API -- that
    // is why #2339 invented none.
    static_assert(std::is_polymorphic_v<Attribute>, "Equals/GetHashCode are virtual");
}

// ===========================================================================
// AttributeTargets
// ===========================================================================

TEST(AttributeTargetsTests, Assembly_IsOne) {
    EXPECT_EQ(static_cast<int>(AttributeTargets::Assembly), 0x0001);
}

TEST(AttributeTargetsTests, Class_IsFour) {
    EXPECT_EQ(static_cast<int>(AttributeTargets::Class), 0x0004);
}

TEST(AttributeTargetsTests, Method_Is0x40) {
    EXPECT_EQ(static_cast<int>(AttributeTargets::Method), 0x0040);
}

TEST(AttributeTargetsTests, All_ContainsClass) {
    EXPECT_NE(static_cast<int>(AttributeTargets::All & AttributeTargets::Class), 0);
}

TEST(AttributeTargetsTests, OrOperator_CombinesFlags) {
    auto combined = AttributeTargets::Class | AttributeTargets::Method;
    EXPECT_NE(static_cast<int>(combined & AttributeTargets::Class), 0);
    EXPECT_NE(static_cast<int>(combined & AttributeTargets::Method), 0);
}

TEST(AttributeTargetsTests, AndOperator_Filters) {
    auto combined = AttributeTargets::Class | AttributeTargets::Method;
    EXPECT_EQ(static_cast<int>(combined & AttributeTargets::Property), 0);
}

// ===========================================================================
// AttributeUsageAttribute
// ===========================================================================

TEST(AttributeUsageAttributeTests, Constructor_StoresValidOn) {
    AttributeUsageAttribute attr(AttributeTargets::Class);
    EXPECT_EQ(attr.getValidOnProperty(), AttributeTargets::Class);
}

TEST(AttributeUsageAttributeTests, AllowMultiple_DefaultFalse) {
    AttributeUsageAttribute attr(AttributeTargets::All);
    EXPECT_FALSE(attr.getAllowMultipleProperty());
}

TEST(AttributeUsageAttributeTests, Inherited_DefaultTrue) {
    AttributeUsageAttribute attr(AttributeTargets::All);
    EXPECT_TRUE(attr.getInheritedProperty());
}

TEST(AttributeUsageAttributeTests, SetAllowMultiple_True) {
    AttributeUsageAttribute attr(AttributeTargets::All);
    attr.setAllowMultipleProperty(true);
    EXPECT_TRUE(attr.getAllowMultipleProperty());
}

TEST(AttributeUsageAttributeTests, SetInherited_False) {
    AttributeUsageAttribute attr(AttributeTargets::All);
    attr.setInheritedProperty(false);
    EXPECT_FALSE(attr.getInheritedProperty());
}

TEST(AttributeUsageAttributeTests, ThreeArgCtor_StoresAll) {
    AttributeUsageAttribute attr(AttributeTargets::Method, true, false);
    EXPECT_EQ(attr.getValidOnProperty(), AttributeTargets::Method);
    EXPECT_TRUE(attr.getAllowMultipleProperty());
    EXPECT_FALSE(attr.getInheritedProperty());
}

TEST(AttributeUsageAttributeTests, Default_TargetsAll) {
    EXPECT_EQ(AttributeUsageAttribute::Default.getValidOnProperty(), AttributeTargets::All);
}

TEST(AttributeUsageAttributeTests, Default_AllowMultipleFalse) {
    EXPECT_FALSE(AttributeUsageAttribute::Default.getAllowMultipleProperty());
}

TEST(AttributeUsageAttributeTests, Default_InheritedTrue) {
    EXPECT_TRUE(AttributeUsageAttribute::Default.getInheritedProperty());
}

// ===========================================================================
// CLSCompliantAttribute
// ===========================================================================

TEST(CLSCompliantAttributeTests, IsCompliant_True) {
    CLSCompliantAttribute attr(true);
    EXPECT_TRUE(attr.getIsCompliantProperty());
}

TEST(CLSCompliantAttributeTests, IsCompliant_False) {
    CLSCompliantAttribute attr(false);
    EXPECT_FALSE(attr.getIsCompliantProperty());
}

// ===========================================================================
// ObsoleteAttribute
// ===========================================================================

TEST(ObsoleteAttributeTests, DefaultCtor_MessageIsAbsent) {
    // #2295: nullopt, not "". A default attribute has NO message in .NET, and now here too.
    ObsoleteAttribute attr;
    EXPECT_EQ(std::nullopt, attr.getMessageProperty());
}

TEST(ObsoleteAttributeTests, MessageCtor_StoresMessage) {
    ObsoleteAttribute attr("Use Foo instead");
    EXPECT_EQ(attr.getMessageProperty(), "Use Foo instead");
}

TEST(ObsoleteAttributeTests, MessageIsError_IsError_True) {
    ObsoleteAttribute attr("Removed", true);
    EXPECT_TRUE(attr.getIsErrorProperty());
}

TEST(ObsoleteAttributeTests, MessageIsError_IsError_False) {
    ObsoleteAttribute attr("Deprecated", false);
    EXPECT_FALSE(attr.getIsErrorProperty());
}

TEST(ObsoleteAttributeTests, DefaultCtor_IsError_False) {
    ObsoleteAttribute attr;
    EXPECT_FALSE(attr.getIsErrorProperty());
}

TEST(ObsoleteAttributeTests, DiagnosticId_DefaultIsAbsent) {
    ObsoleteAttribute attr("msg");
    EXPECT_EQ(std::nullopt, attr.getDiagnosticIdProperty());
}

TEST(ObsoleteAttributeTests, SetDiagnosticId_Stored) {
    ObsoleteAttribute attr("msg");
    attr.setDiagnosticIdProperty("SHARP001");
    EXPECT_EQ(attr.getDiagnosticIdProperty(), "SHARP001");
}

TEST(ObsoleteAttributeTests, SetUrlFormat_Stored) {
    ObsoleteAttribute attr("msg");
    attr.setUrlFormatProperty("https://example.com/{0}");
    EXPECT_EQ(attr.getUrlFormatProperty(), "https://example.com/{0}");
}

// ===========================================================================
// Marker-only attributes (instantiation only)
// ===========================================================================

TEST(MarkerAttributeTests, FlagsAttribute_DefaultCtor) {
    EXPECT_NO_THROW(System::FlagsAttribute{});
}

TEST(MarkerAttributeTests, ParamArrayAttribute_DefaultCtor) {
    EXPECT_NO_THROW(System::ParamArrayAttribute{});
}

TEST(MarkerAttributeTests, NonSerializedAttribute_DefaultCtor) {
    EXPECT_NO_THROW(System::NonSerializedAttribute{});
}

TEST(MarkerAttributeTests, SerializableAttribute_DefaultCtor) {
    EXPECT_NO_THROW(System::SerializableAttribute{});
}

TEST(MarkerAttributeTests, ThreadStaticAttribute_DefaultCtor) {
    EXPECT_NO_THROW(System::ThreadStaticAttribute{});
}

// ===========================================================================
// LoaderOptimization / LoaderOptimizationAttribute
// ===========================================================================

TEST(LoaderOptimizationTests, NotSpecified_IsZero) {
    EXPECT_EQ(static_cast<int>(System::LoaderOptimization::NotSpecified), 0);
}
TEST(LoaderOptimizationTests, SingleDomain_IsOne) {
    EXPECT_EQ(static_cast<int>(System::LoaderOptimization::SingleDomain), 1);
}
TEST(LoaderOptimizationTests, MultiDomain_IsTwo) {
    EXPECT_EQ(static_cast<int>(System::LoaderOptimization::MultiDomain), 2);
}
TEST(LoaderOptimizationTests, MultiDomainHost_IsThree) {
    EXPECT_EQ(static_cast<int>(System::LoaderOptimization::MultiDomainHost), 3);
}
TEST(LoaderOptimizationAttributeTests, Ctor_StoresValue) {
    System::LoaderOptimizationAttribute attr(System::LoaderOptimization::SingleDomain);
    EXPECT_EQ(attr.getValueProperty(), System::LoaderOptimization::SingleDomain);
}
TEST(LoaderOptimizationAttributeTests, Ctor_MultiDomain) {
    System::LoaderOptimizationAttribute attr(System::LoaderOptimization::MultiDomain);
    EXPECT_EQ(attr.getValueProperty(), System::LoaderOptimization::MultiDomain);
}
TEST(LoaderOptimizationAttributeTests, IsA_Attribute) {
    System::LoaderOptimizationAttribute attr(System::LoaderOptimization::NotSpecified);
    EXPECT_NE(dynamic_cast<System::Attribute*>(&attr), nullptr);
}
