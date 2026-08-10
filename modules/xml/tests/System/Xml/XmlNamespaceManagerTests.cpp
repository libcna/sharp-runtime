// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ArgumentException.hpp"
#include "System/Xml/NameTable.hpp"
#include "System/Xml/XmlNamespaceManager.hpp"

using namespace System::Xml;

namespace {
    std::shared_ptr<XmlNameTable> NewNameTable() {
        return std::make_shared<NameTable>();
    }
}

TEST(XmlNamespaceManagerTests, DefaultNamespace_InitiallyEmpty) {
    XmlNamespaceManager mgr(NewNameTable());
    EXPECT_EQ(mgr.getDefaultNamespaceProperty(), "");
}

TEST(XmlNamespaceManagerTests, PredefinedXmlPrefix_ResolvesToFixedNamespace) {
    XmlNamespaceManager mgr(NewNameTable());
    auto ns = mgr.LookupNamespace("xml");
    ASSERT_TRUE(ns.has_value());
    EXPECT_EQ(*ns, "http://www.w3.org/XML/1998/namespace");
}

TEST(XmlNamespaceManagerTests, AddNamespace_ThenLookupNamespace_Resolves) {
    XmlNamespaceManager mgr(NewNameTable());
    mgr.AddNamespace("foo", "urn:foo");
    auto ns = mgr.LookupNamespace("foo");
    ASSERT_TRUE(ns.has_value());
    EXPECT_EQ(*ns, "urn:foo");
}

TEST(XmlNamespaceManagerTests, AddNamespace_XmlnsPrefix_Throws) {
    XmlNamespaceManager mgr(NewNameTable());
    EXPECT_THROW(mgr.AddNamespace("xmlns", "urn:foo"), System::ArgumentException);
}

TEST(XmlNamespaceManagerTests, AddNamespace_XmlPrefixWithWrongUri_Throws) {
    XmlNamespaceManager mgr(NewNameTable());
    EXPECT_THROW(mgr.AddNamespace("xml", "urn:bogus"), System::ArgumentException);
}

TEST(XmlNamespaceManagerTests, AddNamespace_XmlPrefixWithCorrectUri_Allowed) {
    XmlNamespaceManager mgr(NewNameTable());
    EXPECT_NO_THROW(mgr.AddNamespace("xml", "http://www.w3.org/XML/1998/namespace"));
}

TEST(XmlNamespaceManagerTests, LookupPrefix_FindsMatchingUri) {
    XmlNamespaceManager mgr(NewNameTable());
    mgr.AddNamespace("foo", "urn:foo");
    auto prefix = mgr.LookupPrefix("urn:foo");
    ASSERT_TRUE(prefix.has_value());
    EXPECT_EQ(*prefix, "foo");
}

TEST(XmlNamespaceManagerTests, LookupNamespace_UnknownPrefix_ReturnsNullopt) {
    XmlNamespaceManager mgr(NewNameTable());
    EXPECT_FALSE(mgr.LookupNamespace("nope").has_value());
}

TEST(XmlNamespaceManagerTests, PushScope_ThenAddNamespace_ShadowsOuterOnly_WithinScope) {
    XmlNamespaceManager mgr(NewNameTable());
    mgr.AddNamespace("foo", "urn:foo-outer");
    mgr.PushScope();
    mgr.AddNamespace("foo", "urn:foo-inner");
    EXPECT_EQ(*mgr.LookupNamespace("foo"), "urn:foo-inner");
    mgr.PopScope();
    EXPECT_EQ(*mgr.LookupNamespace("foo"), "urn:foo-outer");
}

TEST(XmlNamespaceManagerTests, PopScope_AtRootScope_ReturnsFalse) {
    XmlNamespaceManager mgr(NewNameTable());
    EXPECT_FALSE(mgr.PopScope());
}

// UPDATED BY TICKET #2077, with a recorded reason. This test previously asserted
// `EXPECT_FALSE(mgr.HasNamespace("foo"))` after PushScope -- i.e. it PINNED the
// defect SR-AUD-353 reports, and pinned it against the type's own documented
// contract ("true if @p prefix is declared in any scope") and against its own
// sibling LookupNamespace, which has always searched outward. A test that locks
// in the behaviour a finding identifies as wrong is the reason the finding
// survived; it is corrected here rather than deleted, so the history of the
// claim stays visible.
TEST(XmlNamespaceManagerTests, HasNamespace_ChecksEveryActiveScope) {
    XmlNamespaceManager mgr(NewNameTable());
    mgr.AddNamespace("foo", "urn:foo");
    EXPECT_TRUE(mgr.HasNamespace("foo"));
    mgr.PushScope();
    EXPECT_TRUE(mgr.HasNamespace("foo"))
        << "an outer declaration is still in scope -- SR-AUD-353, ticket #2077";
    EXPECT_EQ(mgr.LookupNamespace("foo").value_or(""), "urn:foo");
}

TEST(XmlNamespaceManagerTests, RemoveNamespace_MatchingPrefixAndUri_Removes) {
    XmlNamespaceManager mgr(NewNameTable());
    mgr.AddNamespace("foo", "urn:foo");
    mgr.RemoveNamespace("foo", "urn:foo");
    EXPECT_FALSE(mgr.LookupNamespace("foo").has_value());
}

TEST(XmlNamespaceManagerTests, GetNamespacesInScope_All_IncludesXmlButNotXmlnsOrEmpty) {
    XmlNamespaceManager mgr(NewNameTable());
    mgr.AddNamespace("foo", "urn:foo");
    auto scope = mgr.GetNamespacesInScope(XmlNamespaceScope::All);
    EXPECT_EQ(scope.count("xml"), 1u);
    EXPECT_EQ(scope.count("xmlns"), 0u);
    EXPECT_EQ(scope.count(""), 0u);
    EXPECT_EQ(scope.at("foo"), "urn:foo");
}

TEST(XmlNamespaceManagerTests, GetNamespacesInScope_ExcludeXml_OmitsXml) {
    XmlNamespaceManager mgr(NewNameTable());
    auto scope = mgr.GetNamespacesInScope(XmlNamespaceScope::ExcludeXml);
    EXPECT_EQ(scope.count("xml"), 0u);
}

// ===========================================================================
// Ticket #2077 -- HasNamespace must search every active scope, as its own
// contract and its own sibling LookupNamespace already do (SR-AUD-353;
// docs/SystemXmlNamespaceReviewPlan.md §4.5).
//
// Before #2077 the predicate inspected scopes_.back() alone, so one manager
// answered HasNamespace("p")=false and LookupNamespace("p")="urn:outer" at the
// same time, and the built-in "xml" prefix it installs itself was invisible from
// any inner scope.
// ===========================================================================

TEST(XmlNamespaceManagerScopeSearchTests, InheritedPrefixIsVisibleFromAnInnerScope) {
    XmlNamespaceManager mgr(NewNameTable());
    mgr.AddNamespace("p", "urn:outer");
    mgr.PushScope();
    EXPECT_TRUE(mgr.HasNamespace("p"));
    mgr.PushScope();
    EXPECT_TRUE(mgr.HasNamespace("p")) << "two scopes deep is still in scope";
}

TEST(XmlNamespaceManagerScopeSearchTests, TheBuiltInPrefixesAreVisibleFromAnyScope) {
    XmlNamespaceManager mgr(NewNameTable());
    mgr.PushScope();
    EXPECT_TRUE(mgr.HasNamespace("xml"));
    EXPECT_TRUE(mgr.HasNamespace("xmlns"));
    EXPECT_TRUE(mgr.HasNamespace(""));
}

TEST(XmlNamespaceManagerScopeSearchTests, ALocallyDeclaredPrefixIsStillVisible) {
    XmlNamespaceManager mgr(NewNameTable());
    mgr.PushScope();
    mgr.AddNamespace("q", "urn:inner");
    EXPECT_TRUE(mgr.HasNamespace("q"));
}

TEST(XmlNamespaceManagerScopeSearchTests, AnUndeclaredPrefixIsStillFalse) {
    XmlNamespaceManager mgr(NewNameTable());
    mgr.AddNamespace("p", "urn:outer");
    mgr.PushScope();
    EXPECT_FALSE(mgr.HasNamespace("nope"));
    EXPECT_FALSE(mgr.HasNamespace("P")) << "prefixes are case-sensitive";
}

TEST(XmlNamespaceManagerScopeSearchTests, APoppedPrefixIsGoneAgain) {
    XmlNamespaceManager mgr(NewNameTable());
    mgr.PushScope();
    mgr.AddNamespace("q", "urn:inner");
    ASSERT_TRUE(mgr.HasNamespace("q"));
    mgr.PopScope();
    EXPECT_FALSE(mgr.HasNamespace("q"));
}

// The property that makes the two members impossible to desynchronise again:
// over a whole scope stack, HasNamespace and LookupNamespace must agree for
// every prefix either of them has ever seen.
TEST(XmlNamespaceManagerScopeSearchTests, HasNamespaceAgreesWithLookupNamespaceEverywhere) {
    XmlNamespaceManager mgr(NewNameTable());
    const std::vector<std::string> probes = {"", "xml", "xmlns", "a", "b", "c", "missing"};

    mgr.AddNamespace("a", "urn:a");
    for (int depth = 0; depth < 4; ++depth) {
        mgr.PushScope();
        mgr.AddNamespace("b", "urn:b" + std::to_string(depth));
        for (const auto& prefix : probes)
            EXPECT_EQ(mgr.HasNamespace(prefix), mgr.LookupNamespace(prefix).has_value())
                << "prefix='" << prefix << "' depth=" << depth;
    }
    while (mgr.PopScope()) {
        for (const auto& prefix : probes)
            EXPECT_EQ(mgr.HasNamespace(prefix), mgr.LookupNamespace(prefix).has_value())
                << "prefix='" << prefix << "' while unwinding";
    }
}
