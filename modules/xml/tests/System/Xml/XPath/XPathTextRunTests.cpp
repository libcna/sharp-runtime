// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2081 / SR-AUD-355 — the XPath 1.0 logical text node.
//
// The XPath data model has ONE text node per maximal run of adjacent text-like DOM siblings,
// whose string-value is their concatenation. XmlDocumentNavigator returned the native node
// directly, so <r>left<![CDATA[right]]></r> presented TWO nodes valued "left" and "right"
// instead of one valued "leftright" — and position(), count() and every count-based
// expression diverged with it. Measured before the repair in
// build-probe/2081_probe1_before.log.
//
// The repair is three helpers modelled on the .NET mechanism the navigator's own doc-comment
// already named (CalibrateText / ValueText): a run's LOGICAL POSITION is its FIRST member, so
// every navigation lands there, every accessor answers from there, and IsSamePosition compares
// there. Adjacency is DOM sibling adjacency — any non-text-like sibling ends a run, including
// one with no XPath representation at all, so a comment, a PI and an entity reference each
// split a run. See docs/SystemXmlNamespaceReviewPlan.md §4.7 and §20.7.
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <algorithm>
#include <vector>

#include "System/Xml/XPath/XPathNavigator.hpp"
#include "System/Xml/XPath/XPathNodeIterator.hpp"
#include "System/Xml/XPath/XPathNodeType.hpp"
#include "System/Xml/IHasXmlNode.hpp"
#include "System/Xml/XmlDocument.hpp"
#include "System/Xml/XmlElement.hpp"

using namespace System::Xml;
using namespace System::Xml::XPath;

namespace {

/// Readable node-type name; numeric codes were a source of error while writing these tests.
std::string TypeName(XPathNodeType t) {
    switch (t) {
        case XPathNodeType::Root:                  return "Root";
        case XPathNodeType::Element:               return "Element";
        case XPathNodeType::Attribute:             return "Attribute";
        case XPathNodeType::Namespace:             return "Namespace";
        case XPathNodeType::Text:                  return "Text";
        case XPathNodeType::SignificantWhitespace: return "SignificantWhitespace";
        case XPathNodeType::Whitespace:            return "Whitespace";
        case XPathNodeType::ProcessingInstruction: return "ProcessingInstruction";
        case XPathNodeType::Comment:               return "Comment";
        case XPathNodeType::All:                   return "All";
    }
    return "?";
}

/// Every child position the navigator visits walking forward, as "Type='value'".
std::vector<std::string> Forward(XmlDocument& d) {
    std::vector<std::string> out;
    auto* r = d.getDocumentElementProperty();
    std::unique_ptr<XPathNavigator> n(r->CreateNavigator());
    if (!n->MoveToFirstChild()) return out;
    do {
        out.push_back(TypeName(n->getNodeTypeProperty()) + "='" + n->getValueProperty() + "'");
    } while (n->MoveToNext());
    return out;
}

/// The same walk driven backwards from the last position, reversed — must equal Forward().
std::vector<std::string> BackwardReversed(XmlDocument& d) {
    std::vector<std::string> out;
    auto* r = d.getDocumentElementProperty();
    std::unique_ptr<XPathNavigator> n(r->CreateNavigator());
    if (!n->MoveToFirstChild()) return out;
    while (n->MoveToNext()) {}
    do {
        out.push_back(TypeName(n->getNodeTypeProperty()) + "='" + n->getValueProperty() + "'");
    } while (n->MoveToPrevious());
    std::reverse(out.begin(), out.end());
    return out;
}

XmlDocument* Load(std::unique_ptr<XmlDocument>& holder, const char* xml) {
    holder = std::make_unique<XmlDocument>();
    holder->LoadXml(xml);
    return holder.get();
}

} // namespace

// ===========================================================================
// #2081 — a run is one node with the concatenated value
// ===========================================================================

TEST(XPathTextRunTests, TextThenCDataIsOneLogicalNode) {
    std::unique_ptr<XmlDocument> h;
    XmlDocument& d = *Load(h, "<r>left<![CDATA[right]]></r>");
    EXPECT_EQ(Forward(d), (std::vector<std::string>{"Text='leftright'"}));
}

TEST(XPathTextRunTests, EveryRunShapeCollapses) {
    struct Case { const char* xml; std::vector<std::string> expected; };
    const std::vector<Case> cases{
        {"<r>left<![CDATA[right]]></r>",                 {"Text='leftright'"}},
        {"<r><![CDATA[left]]>right</r>",                 {"Text='leftright'"}},
        {"<r><![CDATA[a]]><![CDATA[b]]><![CDATA[c]]></r>", {"Text='abc'"}},
        {"<r>a<![CDATA[b]]>c</r>",                       {"Text='abc'"}},
        {"<r>only</r>",                                  {"Text='only'"}},
    };
    for (const auto& c : cases) {
        std::unique_ptr<XmlDocument> h;
        XmlDocument& d = *Load(h, c.xml);
        EXPECT_EQ(Forward(d), c.expected) << c.xml;
    }
}

TEST(XPathTextRunTests, ARunBoundedByElementsOnBothSidesCollapsesToo) {
    std::unique_ptr<XmlDocument> h;
    XmlDocument& d = *Load(h, "<r><a/>x<![CDATA[y]]><b/></r>");
    EXPECT_EQ(Forward(d), (std::vector<std::string>{"Element=''", "Text='xy'", "Element=''"}));
}

TEST(XPathTextRunTests, UnNormalizedTextNodesBuiltByHandAlsoCollapse) {
    XmlDocument d;
    d.LoadXml("<r/>");
    auto* r = d.getDocumentElementProperty();
    r->AppendChild(d.CreateTextNode("t1"));
    r->AppendChild(d.CreateTextNode("t2"));
    r->AppendChild(d.CreateTextNode("t3"));
    EXPECT_EQ(Forward(d), (std::vector<std::string>{"Text='t1t2t3'"}));
}

TEST(XPathTextRunTests, AMixedRunTakesItsRunStartsNodeType) {
    // .NET's CalibrateText positions on the first member and NodeType is that member's, so a
    // Whitespace-led run reports Whitespace with the whole run's value. Recorded, not accidental.
    {
        XmlDocument d;
        d.LoadXml("<r/>");
        auto* r = d.getDocumentElementProperty();
        r->AppendChild(d.CreateWhitespace("  "));
        r->AppendChild(d.CreateTextNode("x"));
        std::unique_ptr<XPathNavigator> n(r->CreateNavigator());
        ASSERT_TRUE(n->MoveToFirstChild());
        EXPECT_EQ(n->getNodeTypeProperty(), XPathNodeType::Whitespace);
        EXPECT_EQ(n->getValueProperty(), "  x");
        EXPECT_FALSE(n->MoveToNext());
    }
    {
        XmlDocument d;
        d.LoadXml("<r/>");
        auto* r = d.getDocumentElementProperty();
        r->AppendChild(d.CreateTextNode("x"));
        r->AppendChild(d.CreateSignificantWhitespace(" "));
        std::unique_ptr<XPathNavigator> n(r->CreateNavigator());
        ASSERT_TRUE(n->MoveToFirstChild());
        EXPECT_EQ(n->getNodeTypeProperty(), XPathNodeType::Text);
        EXPECT_EQ(n->getValueProperty(), "x ");
        EXPECT_FALSE(n->MoveToNext());
    }
}

// ===========================================================================
// #2081 — what must NOT collapse
// ===========================================================================

TEST(XPathTextRunTests, ACommentOrProcessingInstructionSplitsARun) {
    {
        std::unique_ptr<XmlDocument> h;
        XmlDocument& d = *Load(h, "<r>a<!--c-->b</r>");
        EXPECT_EQ(Forward(d), (std::vector<std::string>{"Text='a'", "Comment='c'", "Text='b'"}));
    }
    {
        XmlDocument d;
        d.LoadXml("<r>a</r>");
        auto* r = d.getDocumentElementProperty();
        r->AppendChild(d.CreateProcessingInstruction("pi", "dd"));
        r->AppendChild(d.CreateTextNode("b"));
        EXPECT_EQ(Forward(d), (std::vector<std::string>{"Text='a'", "ProcessingInstruction='dd'", "Text='b'"}));
    }
}

TEST(XPathTextRunTests, ATreeWithNoTextIsUnaffected) {
    std::unique_ptr<XmlDocument> h;
    XmlDocument& d = *Load(h, "<r><a/><b/></r>");
    EXPECT_EQ(Forward(d), (std::vector<std::string>{"Element=''", "Element=''"}));
}

TEST(XPathTextRunTests, ElementAndRootStringValuesAreUnchanged) {
    // These were already the correct concatenation via getInnerTextProperty; the repair must
    // not disturb them.
    std::unique_ptr<XmlDocument> h;
    XmlDocument& d = *Load(h, "<r>a<![CDATA[b]]><c>d</c>e</r>");
    std::unique_ptr<XPathNavigator> n(d.getDocumentElementProperty()->CreateNavigator());
    EXPECT_EQ(n->getValueProperty(), "abde");
}

// ===========================================================================
// #2081 — directional symmetry, which is where a partial fix would break
// ===========================================================================

TEST(XPathTextRunTests, BackwardNavigationIsTheExactMirrorOfForward) {
    for (const char* xml : {"<r>left<![CDATA[right]]></r>",
                            "<r><a/>x<![CDATA[y]]><b/></r>",
                            "<r>a<!--c-->b</r>",
                            "<r>a<![CDATA[b]]>c<d/>e<![CDATA[f]]></r>",
                            "<r><a/><b/></r>",
                            "<r>only</r>"}) {
        std::unique_ptr<XmlDocument> h;
        XmlDocument& d = *Load(h, xml);
        EXPECT_EQ(BackwardReversed(d), Forward(d)) << xml;
    }
}

TEST(XPathTextRunTests, MoveToNextSkipsTheWholeRunNotOneNativeNode) {
    std::unique_ptr<XmlDocument> h;
    XmlDocument& d = *Load(h, "<r>a<![CDATA[b]]>c<d/></r>");
    std::unique_ptr<XPathNavigator> n(d.getDocumentElementProperty()->CreateNavigator());
    ASSERT_TRUE(n->MoveToFirstChild());
    EXPECT_EQ(n->getValueProperty(), "abc");
    ASSERT_TRUE(n->MoveToNext());
    EXPECT_EQ(n->getNodeTypeProperty(), XPathNodeType::Element);
    EXPECT_FALSE(n->MoveToNext());
}

TEST(XPathTextRunTests, MoveToParentFromAnywhereInARunReachesTheSameElement) {
    std::unique_ptr<XmlDocument> h;
    XmlDocument& d = *Load(h, "<r>a<![CDATA[b]]></r>");
    auto* r = d.getDocumentElementProperty();
    std::unique_ptr<XPathNavigator> n(r->CreateNavigator());
    ASSERT_TRUE(n->MoveToFirstChild());
    ASSERT_TRUE(n->MoveToParent());
    EXPECT_EQ(n->getNodeTypeProperty(), XPathNodeType::Element);
    EXPECT_EQ(n->getValueProperty(), "ab");
}

// ===========================================================================
// #2081 — one run is one POSITION, not merely one value
// ===========================================================================

TEST(XPathTextRunTests, TwoNavigatorsOnDifferentMembersOfOneRunAreTheSamePosition) {
    std::unique_ptr<XmlDocument> h;
    XmlDocument& d = *Load(h, "<r>a<![CDATA[b]]></r>");
    auto* r = d.getDocumentElementProperty();
    XmlNode* first  = r->getFirstChildProperty();
    XmlNode* second = first->getNextSiblingProperty();
    ASSERT_NE(second, nullptr);

    std::unique_ptr<XPathNavigator> n1(first->CreateNavigator());
    std::unique_ptr<XPathNavigator> n2(second->CreateNavigator());
    EXPECT_TRUE(n1->IsSamePosition(*n2));
    EXPECT_TRUE(n2->IsSamePosition(*n1));
    EXPECT_EQ(n1->getValueProperty(), "ab");
    EXPECT_EQ(n2->getValueProperty(), "ab");
    auto* h1 = dynamic_cast<System::Xml::IHasXmlNode*>(n1.get());
    auto* h2 = dynamic_cast<System::Xml::IHasXmlNode*>(n2.get());
    ASSERT_NE(h1, nullptr);
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h1->GetNode(), h2->GetNode()) << "the logical node is the run start";
}

TEST(XPathTextRunTests, ANavigatorBuiltDirectlyOnAMidRunNodeIsCalibrated) {
    std::unique_ptr<XmlDocument> h;
    XmlDocument& d = *Load(h, "<r>a<![CDATA[b]]></r>");
    auto* r = d.getDocumentElementProperty();
    XmlNode* second = r->getFirstChildProperty()->getNextSiblingProperty();
    ASSERT_NE(second, nullptr);
    std::unique_ptr<XPathNavigator> n(second->CreateNavigator());
    EXPECT_EQ(n->getValueProperty(), "ab");
    EXPECT_EQ(n->getNodeTypeProperty(), XPathNodeType::Text);
    EXPECT_FALSE(n->MoveToPrevious()) << "the run start has no previous sibling here";
    EXPECT_FALSE(n->MoveToNext()) << "the run is the last child";
}

TEST(XPathTextRunTests, AMidRunNavigatorsNodeTypeComesFromTheRunStart) {
    // Deliberately a run whose members map to DIFFERENT XPath node types. A Text-after-CDATA
    // run cannot discriminate — CDATA maps to Text too — so an uncalibrated getNodeTypeProperty
    // would pass such a test while still being wrong.
    XmlDocument d;
    d.LoadXml("<r/>");
    auto* r = d.getDocumentElementProperty();
    r->AppendChild(d.CreateWhitespace("  "));
    r->AppendChild(d.CreateTextNode("x"));
    XmlNode* second = r->getFirstChildProperty()->getNextSiblingProperty();
    ASSERT_NE(second, nullptr);
    std::unique_ptr<XPathNavigator> n(second->CreateNavigator());
    EXPECT_EQ(n->getNodeTypeProperty(), XPathNodeType::Whitespace)
        << "the run start is the Whitespace node, so the logical node's type is Whitespace";
    EXPECT_EQ(n->getValueProperty(), "  x");
}

// ===========================================================================
// #2081 — the count-based expressions the finding says diverge
// ===========================================================================

TEST(XPathTextRunTests, CountAndPositionSeeOneTextNodePerRun) {
    std::unique_ptr<XmlDocument> h;
    XmlDocument& d = *Load(h, "<r>a<![CDATA[b]]>c<e/>f</r>");
    std::unique_ptr<XPathNavigator> n(d.getDocumentElementProperty()->CreateNavigator());
    std::unique_ptr<XPathNodeIterator> it(n->Select("text()"));
    int count = 0;
    std::vector<std::string> values;
    while (it->MoveNext()) {
        ++count;
        values.push_back(it->getCurrentProperty()->getValueProperty());
    }
    EXPECT_EQ(count, 2) << "one node for the run \"abc\", one for \"f\"";
    EXPECT_EQ(values, (std::vector<std::string>{"abc", "f"}));
}

TEST(XPathTextRunTests, SelectingElementsIsCompletelyUnaffected) {
    std::unique_ptr<XmlDocument> h;
    XmlDocument& d = *Load(h, "<r>a<![CDATA[b]]><c>1</c><c>2</c>d</r>");
    std::unique_ptr<XPathNavigator> n(d.getDocumentElementProperty()->CreateNavigator());
    std::unique_ptr<XPathNodeIterator> it(n->Select("c"));
    std::vector<std::string> values;
    while (it->MoveNext()) values.push_back(it->getCurrentProperty()->getValueProperty());
    EXPECT_EQ(values, (std::vector<std::string>{"1", "2"}));
}
