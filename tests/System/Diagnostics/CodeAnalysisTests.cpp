// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for System::Diagnostics::CodeAnalysis attribute types.
#include <gtest/gtest.h>
#include <string>
#include "System/Diagnostics/CodeAnalysis/CodeAnalysisAttributes.hpp"

using namespace System::Diagnostics::CodeAnalysis;

// ===========================================================================
// Marker-only null-analysis attributes
// ===========================================================================

TEST(CodeAnalysisMarkerTests, NotNullAttribute_DefaultCtor) {
    EXPECT_NO_THROW(NotNullAttribute{});
}

TEST(CodeAnalysisMarkerTests, MaybeNullAttribute_DefaultCtor) {
    EXPECT_NO_THROW(MaybeNullAttribute{});
}

TEST(CodeAnalysisMarkerTests, AllowNullAttribute_DefaultCtor) {
    EXPECT_NO_THROW(AllowNullAttribute{});
}

TEST(CodeAnalysisMarkerTests, DisallowNullAttribute_DefaultCtor) {
    EXPECT_NO_THROW(DisallowNullAttribute{});
}

TEST(CodeAnalysisMarkerTests, DoesNotReturnAttribute_DefaultCtor) {
    EXPECT_NO_THROW(DoesNotReturnAttribute{});
}

// ===========================================================================
// NotNullIfNotNullAttribute
// ===========================================================================

TEST(NotNullIfNotNullAttributeTests, ParameterName_Stored) {
    NotNullIfNotNullAttribute attr("other");
    EXPECT_EQ(attr.getParameterNameProperty(), "other");
}

// ===========================================================================
// MaybeNullWhenAttribute / NotNullWhenAttribute
// ===========================================================================

TEST(MaybeNullWhenAttributeTests, ReturnValue_True) {
    MaybeNullWhenAttribute attr(true);
    EXPECT_TRUE(attr.getReturnValueProperty());
}

TEST(MaybeNullWhenAttributeTests, ReturnValue_False) {
    MaybeNullWhenAttribute attr(false);
    EXPECT_FALSE(attr.getReturnValueProperty());
}

TEST(NotNullWhenAttributeTests, ReturnValue_True) {
    NotNullWhenAttribute attr(true);
    EXPECT_TRUE(attr.getReturnValueProperty());
}

TEST(NotNullWhenAttributeTests, ReturnValue_False) {
    NotNullWhenAttribute attr(false);
    EXPECT_FALSE(attr.getReturnValueProperty());
}

// ===========================================================================
// DoesNotReturnIfAttribute
// ===========================================================================

TEST(DoesNotReturnIfAttributeTests, ParameterValue_True) {
    DoesNotReturnIfAttribute attr(true);
    EXPECT_TRUE(attr.getParameterValueProperty());
}

TEST(DoesNotReturnIfAttributeTests, ParameterValue_False) {
    DoesNotReturnIfAttribute attr(false);
    EXPECT_FALSE(attr.getParameterValueProperty());
}

// ===========================================================================
// MemberNotNullAttribute
// ===========================================================================

TEST(MemberNotNullAttributeTests, Member_Stored) {
    MemberNotNullAttribute attr("_field");
    EXPECT_EQ(attr.getMemberProperty(), "_field");
}

// ===========================================================================
// MemberNotNullWhenAttribute
// ===========================================================================

TEST(MemberNotNullWhenAttributeTests, ReturnValueAndMember_Stored) {
    MemberNotNullWhenAttribute attr(true, "_field");
    EXPECT_TRUE(attr.getReturnValueProperty());
    EXPECT_EQ(attr.getMemberProperty(), "_field");
}

// ===========================================================================
// RequiresUnreferencedCodeAttribute
// ===========================================================================

TEST(RequiresUnreferencedCodeAttributeTests, Message_Stored) {
    RequiresUnreferencedCodeAttribute attr("Reflection used");
    EXPECT_EQ(attr.getMessageProperty(), "Reflection used");
}

TEST(RequiresUnreferencedCodeAttributeTests, Url_Optional_DefaultEmpty) {
    RequiresUnreferencedCodeAttribute attr("msg");
    EXPECT_TRUE(attr.getUrlProperty().empty());
}

TEST(RequiresUnreferencedCodeAttributeTests, Url_Stored) {
    RequiresUnreferencedCodeAttribute attr("msg", "https://example.com");
    EXPECT_EQ(attr.getUrlProperty(), "https://example.com");
}

// ===========================================================================
// RequiresDynamicCodeAttribute
// ===========================================================================

TEST(RequiresDynamicCodeAttributeTests, Message_Stored) {
    RequiresDynamicCodeAttribute attr("Dynamic emit used");
    EXPECT_EQ(attr.getMessageProperty(), "Dynamic emit used");
}

TEST(RequiresDynamicCodeAttributeTests, Url_Stored) {
    RequiresDynamicCodeAttribute attr("msg", "https://docs.example.com");
    EXPECT_EQ(attr.getUrlProperty(), "https://docs.example.com");
}

// ===========================================================================
// ExcludeFromCodeCoverageAttribute
// ===========================================================================

TEST(ExcludeFromCodeCoverageAttributeTests, DefaultCtor_JustificationEmpty) {
    ExcludeFromCodeCoverageAttribute attr;
    EXPECT_TRUE(attr.getJustificationProperty().empty());
}

TEST(ExcludeFromCodeCoverageAttributeTests, Justification_Stored) {
    ExcludeFromCodeCoverageAttribute attr("Stub method");
    EXPECT_EQ(attr.getJustificationProperty(), "Stub method");
}

// ===========================================================================
// SuppressMessageAttribute
// ===========================================================================

TEST(SuppressMessageAttributeTests, CategoryAndCheckId_Stored) {
    SuppressMessageAttribute attr("Performance", "CA1815");
    EXPECT_EQ(attr.getCategoryProperty(), "Performance");
    EXPECT_EQ(attr.getCheckIdProperty(), "CA1815");
}

TEST(SuppressMessageAttributeTests, SetJustification_Stored) {
    SuppressMessageAttribute attr("C", "ID");
    attr.setJustificationProperty("intentional");
    EXPECT_EQ(attr.getJustificationProperty(), "intentional");
}

// ===========================================================================
// StringSyntaxAttribute
// ===========================================================================

TEST(StringSyntaxAttributeTests, Syntax_Stored) {
    StringSyntaxAttribute attr("Regex");
    EXPECT_EQ(attr.getSyntaxProperty(), "Regex");
}

TEST(StringSyntaxAttributeTests, StaticConstants_Correct) {
    EXPECT_EQ(std::string(StringSyntaxAttribute::CompositeFormat), "CompositeFormat");
    EXPECT_EQ(std::string(StringSyntaxAttribute::Regex), "Regex");
    EXPECT_EQ(std::string(StringSyntaxAttribute::Json), "Json");
    EXPECT_EQ(std::string(StringSyntaxAttribute::Uri), "Uri");
}
