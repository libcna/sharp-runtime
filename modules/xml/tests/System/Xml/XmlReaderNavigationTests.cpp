// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// XmlReader navigation, namespace and line-info members added for the XNA intermediate XML
// serializer (CNA plans/plan_xnapipeline_parity.md XNAPP-071): MoveToContent, IsStartElement,
// ReadStartElement(name), Skip, MoveToFirstAttribute, LocalName/Prefix/Depth, LookupNamespace,
// IXmlLineInfo, and the settings-aware Create that prohibits a DOCTYPE the way .NET's default
// XmlReaderSettings does.
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "System/Xml/DtdProcessing.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlNodeType.hpp"
#include "System/Xml/XmlReader.hpp"
#include "System/Xml/XmlReaderSettings.hpp"

using System::Xml::DtdProcessing;
using System::Xml::XmlException;
using System::Xml::XmlNodeType;
using System::Xml::XmlReader;
using System::Xml::XmlReaderSettings;

namespace {

const char* const kDocument =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<XnaContent xmlns:o=\"Cna.Oracle\" xmlns=\"urn:default\">\n"
    "  <!-- leading comment -->\n"
    "  <Asset Type=\"o:Nested\" xmlns:inner=\"urn:inner\">\n"
    "    <Name>n</Name>\n"
    "    <Value>3</Value>1 2<Items>3 4</Items>\n"
    "    <Empty />\n"
    "  </Asset>\n"
    "</XnaContent>";

std::unique_ptr<XmlReader> Open(const char* xml) {
    return std::unique_ptr<XmlReader>(XmlReader::CreateFromString(xml));
}

TEST(XmlReaderNavigationTests, MoveToContentSkipsDeclarationAndComments) {
    auto r = Open(kDocument);
    EXPECT_EQ(r->MoveToContent(), XmlNodeType::Element);
    EXPECT_EQ(r->getNameProperty(), "XnaContent");
    r->Read();
    EXPECT_EQ(r->MoveToContent(), XmlNodeType::Element) << "the comment between the elements is skipped";
    EXPECT_EQ(r->getNameProperty(), "Asset");
}

TEST(XmlReaderNavigationTests, IsStartElementAndReadStartElementByName) {
    auto r = Open(kDocument);
    EXPECT_TRUE(r->IsStartElement("XnaContent"));
    EXPECT_FALSE(r->IsStartElement("Asset"));
    r->ReadStartElement("XnaContent");
    EXPECT_TRUE(r->IsStartElement());
    r->ReadStartElement("Asset");
    EXPECT_EQ(r->MoveToContent(), XmlNodeType::Element);
    EXPECT_EQ(r->getNameProperty(), "Name");
}

TEST(XmlReaderNavigationTests, ReadStartElementWithWrongNameThrowsWithLine) {
    auto r = Open(kDocument);
    try {
        r->ReadStartElement("Asset");
        FAIL() << "expected XmlException";
    } catch (const XmlException& e) {
        EXPECT_EQ(std::string(e.what()), "Element 'Asset' was not found. Line 2, position 0.");
    }
}

TEST(XmlReaderNavigationTests, SkipJumpsPastAnElementAndItsChildren) {
    auto r = Open(kDocument);
    r->ReadStartElement("XnaContent");
    r->ReadStartElement("Asset");
    ASSERT_TRUE(r->IsStartElement("Name"));
    r->Skip();
    EXPECT_EQ(r->MoveToContent(), XmlNodeType::Element);
    EXPECT_EQ(r->getNameProperty(), "Value");
    r->Skip();
    EXPECT_EQ(r->MoveToContent(), XmlNodeType::Text);
    EXPECT_EQ(r->getValueProperty(), "1 2");
    r->Skip();
    EXPECT_TRUE(r->IsStartElement("Items"));
    r->Skip();
    EXPECT_TRUE(r->IsStartElement("Empty"));
    r->Skip();
    EXPECT_EQ(r->MoveToContent(), XmlNodeType::EndElement);
    EXPECT_EQ(r->getNameProperty(), "Asset");
}

TEST(XmlReaderNavigationTests, DepthLocalNamePrefixAndAttributes) {
    auto r = Open("<a><o:b o:x='1' y='2'>t</o:b></a>");
    r->Read();
    EXPECT_EQ(r->getDepthProperty(), 0);
    EXPECT_FALSE(r->getHasAttributesProperty());
    r->Read();
    EXPECT_EQ(r->getDepthProperty(), 1);
    EXPECT_EQ(r->getNameProperty(), "o:b");
    EXPECT_EQ(r->getLocalNameProperty(), "b");
    EXPECT_EQ(r->getPrefixProperty(), "o");
    EXPECT_TRUE(r->getHasAttributesProperty());
    EXPECT_EQ(r->getAttributeCountProperty(), 2);
    ASSERT_TRUE(r->MoveToFirstAttribute());
    EXPECT_EQ(r->getNameProperty(), "o:x");
    EXPECT_EQ(r->getLocalNameProperty(), "x");
    EXPECT_EQ(r->getDepthProperty(), 2) << "an attribute is one level below its element";
    ASSERT_TRUE(r->MoveToNextAttribute());
    EXPECT_EQ(r->getNameProperty(), "y");
    EXPECT_EQ(r->getPrefixProperty(), "");
    EXPECT_TRUE(r->MoveToElement());
    r->Read();
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::Text);
    EXPECT_EQ(r->getDepthProperty(), 2);
}

TEST(XmlReaderNavigationTests, LookupNamespaceWalksTheScopeChain) {
    auto r = Open(kDocument);
    r->ReadStartElement("XnaContent");
    ASSERT_TRUE(r->IsStartElement("Asset"));
    EXPECT_EQ(r->LookupNamespace("o").value_or("<none>"), "Cna.Oracle") << "declared on the parent";
    EXPECT_EQ(r->LookupNamespace("inner").value_or("<none>"), "urn:inner") << "declared on the element itself";
    EXPECT_EQ(r->LookupNamespace("").value_or("<none>"), "urn:default") << "the default namespace";
    EXPECT_FALSE(r->LookupNamespace("nope").has_value());
    EXPECT_EQ(r->LookupNamespace("xml").value_or(""), "http://www.w3.org/XML/1998/namespace");
    r->Read();
    ASSERT_TRUE(r->IsStartElement("Name"));
    EXPECT_EQ(r->LookupNamespace("inner").value_or("<none>"), "urn:inner") << "in scope for descendants";
    r->Read();
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::Text);
    EXPECT_EQ(r->LookupNamespace("o").value_or("<none>"), "Cna.Oracle") << "text resolves through its element";
}

TEST(XmlReaderNavigationTests, LineInfoReportsTheStartLineOfEveryNode) {
    auto r = Open(kDocument);
    EXPECT_TRUE(r->HasLineInfo());
    EXPECT_EQ(r->getLineNumberProperty(), 0) << "no node before the first Read()";
    r->ReadStartElement("XnaContent");
    r->ReadStartElement("Asset");
    ASSERT_TRUE(r->IsStartElement("Name"));
    EXPECT_EQ(r->getLineNumberProperty(), 5);
    EXPECT_EQ(r->getLinePositionProperty(), 0) << "tinyxml2 records lines only";
    r->Skip();
    r->Skip();
    EXPECT_EQ(r->MoveToContent(), XmlNodeType::Text);
    EXPECT_EQ(r->getLineNumberProperty(), 6);
}

TEST(XmlReaderNavigationTests, SettingsProhibitDtdByDefault) {
    XmlReaderSettings settings;
    EXPECT_EQ(settings.DtdProcessing, DtdProcessing::Prohibit);
    try {
        std::unique_ptr<XmlReader> r(XmlReader::Create("<!DOCTYPE x><r/>", settings));
        FAIL() << "expected XmlException";
    } catch (const XmlException& e) {
        EXPECT_EQ(std::string(e.what()).rfind("For security reasons DTD is prohibited in this XML document.", 0), 0u);
    }
    settings.DtdProcessing = DtdProcessing::Ignore;
    std::unique_ptr<XmlReader> ignoring(XmlReader::Create("<!DOCTYPE x><r/>", settings));
    EXPECT_EQ(ignoring->MoveToContent(), XmlNodeType::Element);
    EXPECT_EQ(ignoring->getNameProperty(), "r");
    settings.DtdProcessing = DtdProcessing::Parse;
    std::unique_ptr<XmlReader> parsing(XmlReader::Create("<!DOCTYPE x><r/>", settings));
    ASSERT_TRUE(parsing->Read());
    EXPECT_EQ(parsing->getNodeTypeProperty(), XmlNodeType::DocumentType);
}

TEST(XmlReaderNavigationTests, SettingsIgnoreCommentsAndProcessingInstructions) {
    XmlReaderSettings settings;
    settings.IgnoreComments = true;
    settings.IgnoreProcessingInstructions = true;
    // tinyxml2 accepts a processing instruction at document level only, so the PI sits before
    // the root here.
    std::unique_ptr<XmlReader> r(XmlReader::Create("<?pi d?><r><!--c--><a/></r>", settings));
    ASSERT_TRUE(r->Read());
    EXPECT_EQ(r->getNameProperty(), "r") << "the processing instruction was dropped";
    ASSERT_TRUE(r->Read());
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::Element) << "the comment was dropped";
    EXPECT_EQ(r->getNameProperty(), "a");
}

TEST(XmlReaderNavigationTests, OneArgumentCreateKeepsReportingDoctype) {
    // The pre-existing factory is unchanged: callers that want .NET's default prohibition pass
    // settings explicitly.
    auto r = Open("<!DOCTYPE x><r/>");
    ASSERT_TRUE(r->Read());
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::DocumentType);
}

} // namespace
