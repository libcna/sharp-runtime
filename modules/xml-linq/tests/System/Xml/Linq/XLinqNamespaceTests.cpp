// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #2197 (SR-AUD-334): Xml.Linq discarded namespace URIs on
// parse and emitted malformed XML on serialization.
//
// Measured before the repair (build-probe/2195_probe1_surface.log,
// build-probe/2195_probe2_malformed.log):
//
//   {urn:audit}root with a {urn:audit}attribute   serialized `<root attribute="value"/>`
//   `<p:root xmlns:p="urn:audit"/>`                parsed to LOCAL name `p:root`, URI ""
//   `<root xmlns="urn:d"><child/></root>`          parsed with both URIs ""
//   query by {urn:d}child                          MISSING; query by bare `child` found
//   XAttribute(XNamespace::Xmlns + "p", "urn:x")   serialized `<e p="urn:x"/>` -- the
//                                                  DECLARATION became an ordinary attribute
//   two attributes differing only by namespace     serialized `<r x="1" x="2"/>`, which this
//                                                  runtime's own parser REJECTS
//                                                  (XML_ERROR_PARSING_ATTRIBUTE)
//   getIsNamespaceDeclarationProperty()            0 for a parsed `xmlns:p`, 1 for `xmlns`
//
// So this was never only a fidelity gap: it emitted text that could not be read back, and it
// destroyed namespace declarations built the way .NET builds them.
//
// The root cause was that the converter built every XName from the raw qualified tag text while
// the DOM layer directly underneath already resolved LocalName/Prefix/NamespaceURI -- including
// the two rules easiest to get wrong, which this suite pins explicitly: an unprefixed attribute
// takes NO namespace, and the `xml` prefix is built in.

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "System/Xml/Linq/SaveOptions.hpp"
#include "System/Xml/Linq/XAttribute.hpp"
#include "System/Xml/Linq/XDocument.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XName.hpp"
#include "System/Xml/Linq/XNamespace.hpp"
#include "System/Xml/Linq/XNode.hpp"
#include "System/Xml/Linq/XStreamingElement.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlWriter.hpp"

using System::Xml::Linq::SaveOptions;
using System::Xml::Linq::XAttribute;
using System::Xml::Linq::XDocument;
using System::Xml::Linq::XElement;
using System::Xml::Linq::XName;
using System::Xml::Linq::XNamespace;
using System::Xml::Linq::XNode;
using System::Xml::Linq::XStreamingElement;

namespace {

    constexpr const char* kXmlUri = "http://www.w3.org/XML/1998/namespace";
    constexpr const char* kXmlnsUri = "http://www.w3.org/2000/xmlns/";

    std::string compact(const XElement& e) { return e.ToString(SaveOptions::DisableFormatting); }

    std::string throughWriter(const XElement& e) {
        std::unique_ptr<System::Xml::XmlWriter> w(System::Xml::XmlWriter::CreateToString());
        e.WriteTo(*w);
        return w->ToString();
    }

    std::shared_ptr<XAttribute> attributeNamed(const std::shared_ptr<XElement>& e, const XName& n) {
        return e->Attribute(n);
    }

} // namespace

// --- Parse: the prefix must leave the local name -------------------------------------------

TEST(XLinqNamespaceTests, Parse_PrefixedName_ResolvesToItsUri) {
    auto e = XElement::Parse("<p:root xmlns:p=\"urn:audit\"><p:child/></p:root>");
    EXPECT_EQ(e->getNameProperty().getLocalNameProperty(), "root");
    EXPECT_EQ(e->getNameProperty().getNamespaceNameProperty(), "urn:audit");
    ASSERT_EQ(e->Elements().size(), 1u);
    EXPECT_EQ(e->Elements()[0]->getNameProperty(), XName("urn:audit", "child"));
}

TEST(XLinqNamespaceTests, Parse_DefaultNamespace_AppliesToDescendantElements) {
    auto e = XElement::Parse("<root xmlns=\"urn:d\"><child><grand/></child></root>");
    EXPECT_EQ(e->getNameProperty(), XName("urn:d", "root"));
    ASSERT_EQ(e->Descendants().size(), 2u);
    for (const auto& d : e->Descendants())
        EXPECT_EQ(d->getNameProperty().getNamespaceNameProperty(), "urn:d");
}

TEST(XLinqNamespaceTests, Parse_UnprefixedAttribute_TakesNoNamespaceEvenUnderADefault) {
    // The XML Namespaces rule most often got wrong: a default declaration governs element
    // names only. The DOM layer already implements it; this pins that Linq uses that resolver.
    auto e = XElement::Parse("<root xmlns=\"urn:d\" plain=\"1\"/>");
    EXPECT_EQ(e->getNameProperty(), XName("urn:d", "root"));
    ASSERT_NE(attributeNamed(e, XName("plain")), nullptr);
    EXPECT_EQ(attributeNamed(e, XName("urn:d", "plain")), nullptr);
}

TEST(XLinqNamespaceTests, Parse_PrefixedAttribute_ResolvesToItsUri) {
    auto e = XElement::Parse("<r xmlns:p=\"urn:a\" p:x=\"1\" x=\"2\"/>");
    ASSERT_NE(attributeNamed(e, XName("urn:a", "x")), nullptr);
    EXPECT_EQ(attributeNamed(e, XName("urn:a", "x"))->getValueProperty(), "1");
    ASSERT_NE(attributeNamed(e, XName("x")), nullptr);
    EXPECT_EQ(attributeNamed(e, XName("x"))->getValueProperty(), "2");
}

TEST(XLinqNamespaceTests, Parse_ReservedXmlPrefix_ResolvesWithoutADeclaration) {
    auto e = XElement::Parse("<a xml:lang=\"en\"/>");
    ASSERT_EQ(e->getAttributesProperty().size(), 1u);
    EXPECT_EQ(e->getAttributesProperty()[0]->getNameProperty(), XName(kXmlUri, "lang"));
    EXPECT_FALSE(e->getAttributesProperty()[0]->getIsNamespaceDeclarationProperty());
}

TEST(XLinqNamespaceTests, Parse_PrefixRebinding_IsScopedToItsSubtree) {
    auto e = XElement::Parse("<a xmlns:p=\"urn:one\"><p:b/><c xmlns:p=\"urn:two\"><p:d/></c></a>");
    std::vector<std::string> names;
    for (const auto& d : e->Descendants()) names.push_back(d->getNameProperty().ToString());
    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "{urn:one}b");
    EXPECT_EQ(names[1], "c");
    EXPECT_EQ(names[2], "{urn:two}d");
}

TEST(XLinqNamespaceTests, Parse_DefaultNamespaceUndeclaration_RemovesItForTheSubtree) {
    auto e = XElement::Parse("<a xmlns=\"urn:d\"><b xmlns=\"\"><c/></b></a>");
    EXPECT_EQ(e->getNameProperty(), XName("urn:d", "a"));
    auto b = e->Elements().at(0);
    EXPECT_EQ(b->getNameProperty(), XName("b"));
    EXPECT_EQ(b->Elements().at(0)->getNameProperty(), XName("c"));
}

// --- Parse: declarations survive AS declarations ---------------------------------------------

TEST(XLinqNamespaceTests, Parse_PrefixedDeclaration_IsRecognisedAsANamespaceDeclaration) {
    // Measured pre-repair: 0. The name was the literal local name "xmlns:p", so the check that
    // has always been in getIsNamespaceDeclarationProperty() was simply never handed the shape
    // it tests for.
    auto e = XElement::Parse("<p:root xmlns:p=\"urn:audit\"/>");
    ASSERT_EQ(e->getAttributesProperty().size(), 1u);
    const auto& decl = e->getAttributesProperty()[0];
    EXPECT_EQ(decl->getNameProperty(), XName(kXmlnsUri, "p"));
    EXPECT_TRUE(decl->getIsNamespaceDeclarationProperty());
    EXPECT_EQ(decl->getValueProperty(), "urn:audit");
}

TEST(XLinqNamespaceTests, Parse_DefaultDeclaration_IsRecognisedAsANamespaceDeclaration) {
    auto e = XElement::Parse("<root xmlns=\"urn:d\"/>");
    ASSERT_EQ(e->getAttributesProperty().size(), 1u);
    const auto& decl = e->getAttributesProperty()[0];
    EXPECT_EQ(decl->getNameProperty(), XName("xmlns"));
    EXPECT_TRUE(decl->getIsNamespaceDeclarationProperty());
}

// --- Parse: the DOM-layer decision reaches here too -------------------------------------------

TEST(XLinqNamespaceTests, Fix2083_ParseRejectsAnUndeclaredPrefix) {
    // These two cases used to pin the OPPOSITE, and said exactly why: ".NET's reader rejects an
    // undeclared prefix, but narrowing what this runtime accepts is the open question #2083
    // already owns at the DOM layer; answering it from the Linq side would settle it by
    // accident. Pinned so that a later change to it is a DECISION rather than a side effect."
    //
    // #2083 has now been answered at the DOM layer, from the reference
    // (XmlTextReaderImpl.cs:7787, Strings.resx Xml_UnknownNs). XElement::Parse goes through
    // XDocument::Parse and the shared loader, so the decision reaches here -- which is the
    // consistent outcome those pins were protecting, and this is the change that updates them.
    EXPECT_THROW((void)XElement::Parse("<p:root/>"), System::Xml::XmlException);
    EXPECT_THROW((void)XElement::Parse("<r p:x=\"1\"/>"), System::Xml::XmlException);
}

TEST(XLinqNamespaceTests, Fix2083_ParseStillAcceptsADeclaredPrefix) {
    // The invariance row: only the UNDECLARED case moved.
    auto e = XElement::Parse("<p:root xmlns:p=\"urn:x\"/>");
    ASSERT_NE(e, nullptr);
    auto withAttribute = XElement::Parse("<r xmlns:p=\"urn:x\" p:x=\"1\"/>");
    ASSERT_EQ(withAttribute->getAttributesProperty().size(), 2u);
}

// --- Serialize: qualified names and their declarations ----------------------------------------

TEST(XLinqNamespaceTests, Serialize_ProgrammaticNamespacedElement_CarriesAPrefixAndItsDeclaration) {
    XNamespace ns = XNamespace::Get("urn:audit");
    auto root = std::make_shared<XElement>(ns + "root");
    root->Add(std::make_shared<XAttribute>(ns + "attribute", "value"));
    EXPECT_EQ(compact(*root), "<p1:root xmlns:p1=\"urn:audit\" p1:attribute=\"value\"/>");
}

TEST(XLinqNamespaceTests, Serialize_TwoNamespacesWithTheSameLocalName_AreDistinguishable) {
    // Pre-repair: `<r><x/><x/></r>` -- indistinguishable.
    auto r = std::make_shared<XElement>(XName("r"));
    r->Add(std::make_shared<XElement>(XNamespace::Get("urn:a") + "x"));
    r->Add(std::make_shared<XElement>(XNamespace::Get("urn:b") + "x"));
    auto back = XElement::Parse(compact(*r));
    ASSERT_EQ(back->Elements().size(), 2u);
    EXPECT_EQ(back->Elements()[0]->getNameProperty(), XName("urn:a", "x"));
    EXPECT_EQ(back->Elements()[1]->getNameProperty(), XName("urn:b", "x"));
}

TEST(XLinqNamespaceTests, Serialize_TwoAttributesDifferingOnlyByNamespace_ProduceParseableText) {
    // Pre-repair this emitted `<r x="1" x="2"/>`, which this runtime's own parser rejects with
    // XML_ERROR_PARSING_ATTRIBUTE -- silent corruption at a public door, not a fidelity gap.
    auto r = std::make_shared<XElement>(XName("r"));
    r->Add(std::make_shared<XAttribute>(XNamespace::Get("urn:a") + "x", "1"));
    r->Add(std::make_shared<XAttribute>(XNamespace::Get("urn:b") + "x", "2"));

    std::shared_ptr<XElement> back;
    ASSERT_NO_THROW(back = XElement::Parse(compact(*r)));
    ASSERT_NE(attributeNamed(back, XName("urn:a", "x")), nullptr);
    ASSERT_NE(attributeNamed(back, XName("urn:b", "x")), nullptr);
    EXPECT_EQ(attributeNamed(back, XName("urn:a", "x"))->getValueProperty(), "1");
    EXPECT_EQ(attributeNamed(back, XName("urn:b", "x"))->getValueProperty(), "2");
}

TEST(XLinqNamespaceTests, Serialize_PrefixedDeclarationAttribute_StaysADeclaration) {
    // Pre-repair: `<e p="urn:x"/>` -- the declaration silently became an ordinary attribute
    // named `p`, unbinding the prefix for the whole subtree.
    auto e = std::make_shared<XElement>(XName("e"));
    e->Add(std::make_shared<XAttribute>(XNamespace::Xmlns + "p", "urn:x"));
    EXPECT_EQ(compact(*e), "<e xmlns:p=\"urn:x\"/>");
}

TEST(XLinqNamespaceTests, Serialize_DefaultDeclarationAttribute_StaysADeclaration) {
    auto e = std::make_shared<XElement>(XName("e"));
    e->Add(std::make_shared<XAttribute>(XName("xmlns"), "urn:y"));
    // An unqualified element carrying its own non-empty default declaration is a tree XML
    // cannot express -- the declaration governs the element's own name. The explicit
    // declaration wins and no xmlns="" undeclaration is generated beside it, because
    // `<e xmlns="" xmlns="urn:y"/>` is not well-formed.
    EXPECT_EQ(compact(*e), "<e xmlns=\"urn:y\"/>");
}

TEST(XLinqNamespaceTests, Serialize_ADeclaredPrefixIsReusedRatherThanRegenerated) {
    auto e = std::make_shared<XElement>(XNamespace::Get("urn:a") + "root");
    e->Add(std::make_shared<XAttribute>(XNamespace::Xmlns + "keep", "urn:a"));
    EXPECT_EQ(compact(*e), "<keep:root xmlns:keep=\"urn:a\"/>");
}

TEST(XLinqNamespaceTests, Serialize_ADescendantReusesAnAncestorsDeclaration) {
    auto root = XElement::Parse("<p:a xmlns:p=\"urn:one\"><p:b/></p:a>");
    EXPECT_EQ(compact(*root), "<p:a xmlns:p=\"urn:one\"><p:b/></p:a>");
}

TEST(XLinqNamespaceTests, Serialize_AnUnqualifiedChildUnderADefaultNamespace_UndeclaresIt) {
    auto root = std::make_shared<XElement>(XName("urn:d", "root"));
    root->Add(std::make_shared<XAttribute>(XName("xmlns"), "urn:d"));
    root->Add(std::make_shared<XElement>(XName("plain")));
    const std::string text = compact(*root);
    EXPECT_NE(text.find("<plain xmlns=\"\"/>"), std::string::npos) << text;

    auto back = XElement::Parse(text);
    EXPECT_EQ(back->Elements().at(0)->getNameProperty(), XName("plain"));
}

TEST(XLinqNamespaceTests, Serialize_AGeneratedPrefixNeverCollidesWithADeclaredOne) {
    auto e = std::make_shared<XElement>(XNamespace::Get("urn:generated") + "root");
    e->Add(std::make_shared<XAttribute>(XNamespace::Xmlns + "p1", "urn:taken"));
    const std::string text = compact(*e);
    EXPECT_NE(text.find("xmlns:p1=\"urn:taken\""), std::string::npos) << text;
    EXPECT_NE(text.find("xmlns:p2=\"urn:generated\""), std::string::npos) << text;
    EXPECT_EQ(text.rfind("<p2:root", 0), 0u) << text;
}

TEST(XLinqNamespaceTests, Serialize_AnAttributeNeverTakesTheDefaultNamespacesPrefix) {
    // The serialization half of the rule pinned on the parse side by
    // Parse_UnprefixedAttribute_TakesNoNamespaceEvenUnderADefault, and the one a mutation
    // showed the rest of this suite did NOT catch: with the default declaration allowed for an
    // attribute, `{urn:d}x` would render as the bare `x="1"`, which on read-back means an
    // attribute in NO namespace. A separate prefix is mandatory even though the default
    // declaration already names this exact URI.
    auto e = std::make_shared<XElement>(XName("urn:d", "root"));
    e->Add(std::make_shared<XAttribute>(XName("xmlns"), "urn:d"));
    e->Add(std::make_shared<XAttribute>(XName("urn:d", "x"), "1"));
    const std::string text = compact(*e);
    EXPECT_EQ(text.find(" x=\"1\""), std::string::npos) << text;

    auto back = XElement::Parse(text);
    EXPECT_EQ(back->getNameProperty(), XName("urn:d", "root"));
    ASSERT_NE(attributeNamed(back, XName("urn:d", "x")), nullptr) << text;
    EXPECT_EQ(attributeNamed(back, XName("x")), nullptr) << text;
}

TEST(XLinqNamespaceTests, Serialize_TheXmlPrefixIsNeverDeclared) {
    auto e = std::make_shared<XElement>(XName("a"));
    e->Add(std::make_shared<XAttribute>(XName(kXmlUri, "lang"), "en"));
    const std::string text = compact(*e);
    EXPECT_EQ(text, "<a xml:lang=\"en\"/>");
    EXPECT_EQ(text.find("xmlns:"), std::string::npos) << text;
}

TEST(XLinqNamespaceTests, Serialize_UnqualifiedTreesAreByteIdenticalToBefore) {
    // The repair must be invisible to every tree that uses no namespace at all.
    auto e = std::make_shared<XElement>(XName("root"));
    e->Add(std::make_shared<XAttribute>(XName("id"), "1"));
    e->Add(std::make_shared<XElement>(XName("child")));
    EXPECT_EQ(compact(*e), "<root id=\"1\"><child/></root>");
}

// --- Round trips -------------------------------------------------------------------------------

TEST(XLinqNamespaceTests, RoundTrip_PrefixedTree_PreservesNamesAndIsDeepEqual) {
    auto original = XElement::Parse("<p:root xmlns:p=\"urn:a\" p:x=\"1\"><p:kid>t</p:kid></p:root>");
    auto back = XElement::Parse(compact(*original));
    EXPECT_EQ(back->getNameProperty(), XName("urn:a", "root"));
    EXPECT_TRUE(XNode::DeepEquals(original.get(), back.get()));
}

TEST(XLinqNamespaceTests, RoundTrip_DefaultNamespaceTree_PreservesNamesAndIsDeepEqual) {
    auto original = XElement::Parse("<root xmlns=\"urn:d\"><child a=\"1\"/></root>");
    auto back = XElement::Parse(compact(*original));
    EXPECT_EQ(back->getNameProperty(), XName("urn:d", "root"));
    EXPECT_EQ(back->Elements().at(0)->getNameProperty(), XName("urn:d", "child"));
    EXPECT_TRUE(XNode::DeepEquals(original.get(), back.get()));
}

TEST(XLinqNamespaceTests, RoundTrip_ProgrammaticTree_PreservesNamesButGainsItsDeclaration) {
    // A programmatically built tree carries no declaration attribute; serialization must add
    // one, so the reparsed tree has one attribute more and is deliberately NOT DeepEquals to
    // the original. Namespace declarations are attributes, and DeepEquals compares attributes.
    // The names -- the thing SR-AUD-334 was about -- do survive.
    auto original = std::make_shared<XElement>(XNamespace::Get("urn:a") + "root");
    auto back = XElement::Parse(compact(*original));
    EXPECT_EQ(back->getNameProperty(), XName("urn:a", "root"));
    EXPECT_EQ(original->getAttributesProperty().size(), 0u);
    EXPECT_EQ(back->getAttributesProperty().size(), 1u);
    EXPECT_TRUE(back->getAttributesProperty()[0]->getIsNamespaceDeclarationProperty());
    EXPECT_FALSE(XNode::DeepEquals(original.get(), back.get()));
    // ...and a second round trip is stable, which is the property that actually matters.
    auto again = XElement::Parse(compact(*back));
    EXPECT_TRUE(XNode::DeepEquals(back.get(), again.get()));
}

TEST(XLinqNamespaceTests, RoundTrip_RebindingTree_IsStable) {
    auto original = XElement::Parse("<a xmlns:p=\"urn:one\"><p:b/><c xmlns:p=\"urn:two\"><p:d/></c></a>");
    auto back = XElement::Parse(compact(*original));
    EXPECT_TRUE(XNode::DeepEquals(original.get(), back.get()));
    EXPECT_EQ(back->Descendants().size(), 3u);
}

TEST(XLinqNamespaceTests, RoundTrip_ThroughTheDocumentDoor) {
    auto doc = std::make_shared<XDocument>();
    doc->Add(std::make_shared<XElement>(XNamespace::Get("urn:audit") + "root"));
    EXPECT_EQ(doc->ToString(SaveOptions::DisableFormatting), "<p1:root xmlns:p1=\"urn:audit\"/>");
    auto back = XDocument::Parse(doc->ToString(SaveOptions::DisableFormatting));
    ASSERT_NE(back->getRootProperty(), nullptr);
    EXPECT_EQ(back->getRootProperty()->getNameProperty(), XName("urn:audit", "root"));
}

TEST(XLinqNamespaceTests, RoundTrip_ThroughSaveToFile) {
    const std::string path = "build-tmp/2197_ns_probe.xml";
    auto root = std::make_shared<XElement>(XNamespace::Get("urn:file") + "root");
    root->Add(std::make_shared<XAttribute>(XNamespace::Get("urn:file") + "a", "1"));
    ASSERT_NO_THROW(root->Save(path, SaveOptions::DisableFormatting));
    auto back = XElement::Load(path);
    ASSERT_NE(back, nullptr);
    EXPECT_EQ(back->getNameProperty(), XName("urn:file", "root"));
    ASSERT_NE(attributeNamed(back, XName("urn:file", "a")), nullptr);
    std::remove(path.c_str());
}

// --- The two doors must agree ------------------------------------------------------------------

TEST(XLinqNamespaceTests, BothSerializationDoorsProduceTheSameQualifiedNames) {
    auto root = std::make_shared<XElement>(XNamespace::Get("urn:audit") + "root");
    root->Add(std::make_shared<XAttribute>(XNamespace::Get("urn:audit") + "attribute", "value"));
    EXPECT_EQ(throughWriter(*root), compact(*root));
}

TEST(XLinqNamespaceTests, TheWriterDoorAlsoPreservesADeclarationAsADeclaration) {
    auto e = std::make_shared<XElement>(XName("e"));
    e->Add(std::make_shared<XAttribute>(XNamespace::Xmlns + "p", "urn:x"));
    EXPECT_EQ(throughWriter(*e), "<e xmlns:p=\"urn:x\"/>");
}

// --- Namespace-aware query ----------------------------------------------------------------------

TEST(XLinqNamespaceTests, Query_ByQualifiedNameMatchesAndByLocalNameDoesNot) {
    auto e = XElement::Parse("<root xmlns=\"urn:d\"><child/></root>");
    EXPECT_NE(e->Element(XName("urn:d", "child")), nullptr);
    EXPECT_EQ(e->Element(XName("child")), nullptr);
    EXPECT_EQ(e->Elements(XName("urn:d", "child")).size(), 1u);
    EXPECT_EQ(e->Descendants(XName("urn:d", "child")).size(), 1u);
}

// --- GetDefaultNamespace / GetPrefixOfNamespace --------------------------------------------------

TEST(XLinqNamespaceTests, GetDefaultNamespace_ResolvesThroughAncestors) {
    auto root = XElement::Parse("<a xmlns=\"urn:d\"><b><c/></b></a>");
    auto b = root->Elements().at(0);
    auto c = b->Elements().at(0);
    EXPECT_EQ(root->GetDefaultNamespace(), XNamespace::Get("urn:d"));
    EXPECT_EQ(b->GetDefaultNamespace(), XNamespace::Get("urn:d"));
    EXPECT_EQ(c->GetDefaultNamespace(), XNamespace::Get("urn:d"));
}

TEST(XLinqNamespaceTests, GetDefaultNamespace_HonoursAnUndeclaration) {
    auto root = XElement::Parse("<a xmlns=\"urn:d\"><b xmlns=\"\"/></a>");
    EXPECT_EQ(root->GetDefaultNamespace(), XNamespace::Get("urn:d"));
    EXPECT_EQ(root->Elements().at(0)->GetDefaultNamespace(), XNamespace::None);
}

TEST(XLinqNamespaceTests, GetDefaultNamespace_IsNoneWhenNoneIsDeclared) {
    auto e = std::make_shared<XElement>(XName("a"));
    EXPECT_EQ(e->GetDefaultNamespace(), XNamespace::None);
}

TEST(XLinqNamespaceTests, GetPrefixOfNamespace_ResolvesThroughAncestorsAndHonoursRebinding) {
    auto root = XElement::Parse("<a xmlns:p=\"urn:one\"><c xmlns:p=\"urn:two\"><d/></c></a>");
    auto c = root->Elements().at(0);
    auto d = c->Elements().at(0);
    EXPECT_EQ(root->GetPrefixOfNamespace(XNamespace::Get("urn:one")), "p");
    EXPECT_EQ(c->GetPrefixOfNamespace(XNamespace::Get("urn:two")), "p");
    EXPECT_EQ(d->GetPrefixOfNamespace(XNamespace::Get("urn:two")), "p");
    // Shadowed: `p` no longer names urn:one inside `c`, and no other prefix does.
    EXPECT_EQ(c->GetPrefixOfNamespace(XNamespace::Get("urn:one")), "");
}

TEST(XLinqNamespaceTests, GetPrefixOfNamespace_KnowsTheReservedXmlPrefix) {
    auto e = std::make_shared<XElement>(XName("a"));
    EXPECT_EQ(e->GetPrefixOfNamespace(XNamespace::Get(kXmlUri)), "xml");
}

TEST(XLinqNamespaceTests, GetPrefixOfNamespace_IsEmptyForAnUndeclaredNamespace) {
    auto e = std::make_shared<XElement>(XName("a"));
    EXPECT_EQ(e->GetPrefixOfNamespace(XNamespace::Get("urn:nope")), "");
}

// --- XAttribute::ToString standalone ---------------------------------------------------------

TEST(XLinqNamespaceTests, AttributeToString_UnqualifiedIsUnchanged) {
    EXPECT_EQ(XAttribute(XName("x"), "1").ToString(), "x=\"1\"");
}

TEST(XLinqNamespaceTests, AttributeToString_DeclarationRendersAsItself) {
    EXPECT_EQ(XAttribute(XNamespace::Xmlns + "p", "urn:x").ToString(), "xmlns:p=\"urn:x\"");
    EXPECT_EQ(XAttribute(XName("xmlns"), "urn:y").ToString(), "xmlns=\"urn:y\"");
}

TEST(XLinqNamespaceTests, AttributeToString_DetachedQualifiedCarriesItsOwnDeclaration) {
    EXPECT_EQ(XAttribute(XName("urn:a", "x"), "1").ToString(),
              "p1:x=\"1\" xmlns:p1=\"urn:a\"");
}

TEST(XLinqNamespaceTests, AttributeToString_AttachedQualifiedUsesTheOwnersPrefix) {
    auto e = std::make_shared<XElement>(XName("e"));
    e->Add(std::make_shared<XAttribute>(XNamespace::Xmlns + "own", "urn:a"));
    auto a = std::make_shared<XAttribute>(XName("urn:a", "x"), "1");
    e->Add(a);
    EXPECT_EQ(a->ToString(), "own:x=\"1\"");
}

TEST(XLinqNamespaceTests, AttributeToString_XmlPrefixIsUsedWithoutADeclaration) {
    EXPECT_EQ(XAttribute(XName(kXmlUri, "lang"), "en").ToString(), "xml:lang=\"en\"");
}

// --- XStreamingElement ---------------------------------------------------------------------------

TEST(XLinqNamespaceTests, StreamingElement_NamespacedNameCarriesAPrefixAndItsDeclaration) {
    XStreamingElement se(XNamespace::Get("urn:s") + "root");
    se.Add(std::make_shared<XAttribute>(XNamespace::Get("urn:s") + "a", "1"));
    EXPECT_EQ(se.ToString(), "<p1:root xmlns:p1=\"urn:s\" p1:a=\"1\"/>");
}

TEST(XLinqNamespaceTests, StreamingElement_ADeclarationInItsContentIsUsedRatherThanRegenerated) {
    XStreamingElement se(XNamespace::Get("urn:s") + "root");
    se.Add(std::make_shared<XAttribute>(XNamespace::Xmlns + "s", "urn:s"));
    EXPECT_EQ(se.ToString(), "<s:root xmlns:s=\"urn:s\"/>");
}

TEST(XLinqNamespaceTests, StreamingElement_UnqualifiedIsUnchanged) {
    XStreamingElement se(XName("root"));
    se.Add(std::make_shared<XAttribute>(XName("id"), "1"));
    EXPECT_EQ(se.ToString(), "<root id=\"1\"/>");
}

// --- Namespace-declaration validation, reached for the first time by parsed input -----------------

TEST(XLinqNamespaceTests, Parse_PrefixUndeclarationIsRejectedByTheDeclarationValidator) {
    // XML Namespaces 1.0 forbids undeclaring a prefix (`xmlns:p=""`); only 1.1 allows it.
    // XAttribute's ValidateAttribute has always enforced that, but before #2197 no parsed
    // declaration ever reached it, because parsed names never carried the xmlns namespace.
    // Recorded as a behaviour change: this input used to parse and now throws.
    EXPECT_THROW((void)XElement::Parse("<a xmlns:p=\"\"/>"), System::ArgumentException);
}

TEST(XLinqNamespaceTests, Parse_TheLegalXmlPrefixDeclarationIsStillAccepted) {
    EXPECT_NO_THROW((void)XElement::Parse("<a xmlns:xml=\"http://www.w3.org/XML/1998/namespace\"/>"));
}
