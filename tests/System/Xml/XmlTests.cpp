// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// XmlReader/XmlWriter are backed by vendored tinyxml2 (see XmlReader.cpp/XmlWriter.cpp).
// System::Xml::Linq types (XName, XAttribute, XElement, XDocument) are fully implemented,
// including real Parse()/Load() (tinyxml2-backed) and Save(). See XLinqNodeTests.cpp for the
// full XObject/XNode/XContainer/XText/XComment/XCData/XProcessingInstruction/XDocumentType/
// XStreamingElement/comparer/Extensions coverage. See XmlDomTests.cpp for the classic
// XmlDocument DOM API and XmlSupportTests.cpp for XmlException/XmlConvert/XmlQualifiedName/enum
// coverage.
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include "System/NotImplementedException.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlReader.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "System/Xml/Linq/XName.hpp"
#include "System/Xml/Linq/XAttribute.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XDocument.hpp"

using System::NotImplementedException;
using System::Xml::ReadState;
using System::Xml::XmlException;
using System::Xml::XmlNodeType;
using System::Xml::XmlReader;
using System::Xml::XmlWriter;
using System::Xml::Linq::XName;
using System::Xml::Linq::XAttribute;
using System::Xml::Linq::XElement;
using System::Xml::Linq::XDeclaration;
using System::Xml::Linq::XDocument;

// ===========================================================================
// ReadState enum
// ===========================================================================

TEST(ReadStateTests, Initial_IsZero) {
    EXPECT_EQ(static_cast<int>(ReadState::Initial), 0);
}

TEST(ReadStateTests, Interactive_IsOne) {
    EXPECT_EQ(static_cast<int>(ReadState::Interactive), 1);
}

TEST(ReadStateTests, EndOfFile_IsThree) {
    EXPECT_EQ(static_cast<int>(ReadState::EndOfFile), 3);
}

TEST(ReadStateTests, Closed_IsFour) {
    EXPECT_EQ(static_cast<int>(ReadState::Closed), 4);
}

// ===========================================================================
// XmlNodeType enum
// ===========================================================================

TEST(XmlNodeTypeTests, None_IsZero) {
    EXPECT_EQ(static_cast<int>(XmlNodeType::None), 0);
}

TEST(XmlNodeTypeTests, Element_IsOne) {
    EXPECT_EQ(static_cast<int>(XmlNodeType::Element), 1);
}

TEST(XmlNodeTypeTests, Text_IsThree) {
    EXPECT_EQ(static_cast<int>(XmlNodeType::Text), 3);
}

TEST(XmlNodeTypeTests, Comment_IsEight) {
    EXPECT_EQ(static_cast<int>(XmlNodeType::Comment), 8);
}

TEST(XmlNodeTypeTests, XmlDeclaration_Is17) {
    EXPECT_EQ(static_cast<int>(XmlNodeType::XmlDeclaration), 17);
}

// ===========================================================================
// XmlReader — tinyxml2-backed implementation
// ===========================================================================

static const char* kSimpleXml =
    "<root attr=\"hello\"><child>text</child><empty/></root>";

TEST(XmlReaderTests, CreateFromString_DoesNotThrow) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString(kSimpleXml));
    EXPECT_NE(r, nullptr);
}

TEST(XmlReaderTests, FirstRead_ReturnsTrueAndElementNode) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString(kSimpleXml));
    EXPECT_TRUE(r->Read());
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::Element);
}

TEST(XmlReaderTests, FirstElement_NameIsRoot) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString(kSimpleXml));
    r->Read();
    EXPECT_EQ(r->getNameProperty(), "root");
}

TEST(XmlReaderTests, GetAttribute_ReturnsValue) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString(kSimpleXml));
    r->Read(); // <root>
    EXPECT_EQ(r->GetAttribute("attr"), "hello");
}

TEST(XmlReaderTests, GetAttribute_Missing_ReturnsEmpty) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString(kSimpleXml));
    r->Read();
    EXPECT_EQ(r->GetAttribute("nonexistent"), "");
}

TEST(XmlReaderTests, ReadSequence_ChildElement) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString(kSimpleXml));
    r->Read(); // <root>
    r->Read(); // <child>
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::Element);
    EXPECT_EQ(r->getNameProperty(), "child");
}

TEST(XmlReaderTests, ReadSequence_TextNode) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString(kSimpleXml));
    r->Read(); // <root>
    r->Read(); // <child>
    r->Read(); // text
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::Text);
    EXPECT_EQ(r->getValueProperty(), "text");
}

TEST(XmlReaderTests, ReadSequence_EndElement) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString(kSimpleXml));
    r->Read(); // <root>
    r->Read(); // <child>
    r->Read(); // text
    r->Read(); // </child>
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::EndElement);
    EXPECT_EQ(r->getNameProperty(), "child");
}

TEST(XmlReaderTests, EmptyElement_IsEmptyElement) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString(kSimpleXml));
    r->Read(); // <root>
    r->Read(); // <child>
    r->Read(); // text
    r->Read(); // </child>
    r->Read(); // <empty/>
    EXPECT_EQ(r->getNameProperty(), "empty");
    EXPECT_TRUE(r->getIsEmptyElementProperty());
}

TEST(XmlReaderTests, ReadPastEnd_ReturnsFalse) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString("<x/>"));
    while (r->Read()) {}
    EXPECT_FALSE(r->Read());
    EXPECT_EQ(r->getReadStateProperty(), ReadState::EndOfFile);
}

// Regression test for a wave-3 audit finding: most accessors only guarded pos < 0, not
// pos >= events.size() -- after Read() returns false at EOF, pos sits exactly at
// events.size(), so every one of these indexed events[pos] out of bounds (UB/crash) instead
// of returning a safe default. Only getNodeTypeProperty() had the correct upper-bound check.
TEST(XmlReaderTests, AccessorsAfterEOF_ReturnSafeDefaults_DoNotCrash) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString("<x a=\"1\"><y/></x>"));
    while (r->Read()) {}
    ASSERT_EQ(r->getReadStateProperty(), ReadState::EndOfFile);

    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::None);
    EXPECT_EQ(r->getNameProperty(), "");
    EXPECT_EQ(r->getValueProperty(), "");
    EXPECT_FALSE(r->getIsEmptyElementProperty());
    EXPECT_FALSE(r->MoveToElement());
    EXPECT_FALSE(r->MoveToNextAttribute());
    EXPECT_EQ(r->GetAttribute("a"), "");
    EXPECT_THROW(r->ReadStartElement(), XmlException);
    EXPECT_THROW(r->ReadEndElement(), XmlException);
}

TEST(XmlReaderTests, InitialState_IsInitial) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString("<x/>"));
    EXPECT_EQ(r->getReadStateProperty(), ReadState::Initial);
}

TEST(XmlReaderTests, Close_DoesNotThrow) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString("<x/>"));
    EXPECT_NO_THROW(r->Close());
}

TEST(XmlReaderTests, MoveToNextAttribute_IteratesAttributes) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString("<el a=\"1\" b=\"2\"/>"));
    r->Read(); // <el>
    EXPECT_TRUE(r->MoveToNextAttribute());
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::Attribute);
    EXPECT_EQ(r->getNameProperty(), "a");
    EXPECT_EQ(r->getValueProperty(), "1");
    EXPECT_TRUE(r->MoveToNextAttribute());
    EXPECT_EQ(r->getNameProperty(), "b");
    EXPECT_FALSE(r->MoveToNextAttribute()); // no more
}

TEST(XmlReaderTests, MoveToElement_AfterAttributes) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString("<el a=\"1\"/>"));
    r->Read();
    r->MoveToNextAttribute();
    EXPECT_TRUE(r->MoveToElement());
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::Element);
}

TEST(XmlReaderTests, XmlDeclaration_NodeType) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString(
        "<?xml version=\"1.0\"?><root/>"));
    r->Read(); // declaration
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::XmlDeclaration);
}

TEST(XmlReaderTests, ReadElementContentAsString_ReturnsText) {
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString("<msg>hello world</msg>"));
    r->Read(); // <msg>
    std::string text = r->ReadElementContentAsString();
    EXPECT_EQ(text, "hello world");
}

// ===========================================================================
// XmlWriter — tinyxml2-backed implementation
// ===========================================================================

TEST(XmlWriterTests, CreateToString_DoesNotThrow) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    EXPECT_NE(w, nullptr);
}

TEST(XmlWriterTests, SimpleElement_ToString_ContainsTag) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    w->WriteStartElement("root");
    w->WriteEndElement();
    EXPECT_NE(w->ToString().find("<root"), std::string::npos);
}

TEST(XmlWriterTests, WriteAttributeString_InOutput) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    w->WriteStartElement("el");
    w->WriteAttributeString("key", "val");
    w->WriteEndElement();
    std::string out = w->ToString();
    EXPECT_NE(out.find("key=\"val\""), std::string::npos);
}

TEST(XmlWriterTests, WriteString_InOutput) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    w->WriteStartElement("msg");
    w->WriteString("hello");
    w->WriteEndElement();
    EXPECT_NE(w->ToString().find("hello"), std::string::npos);
}

TEST(XmlWriterTests, WriteElementString_ShortForm) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    w->WriteStartElement("root");
    w->WriteElementString("name", "Alice");
    w->WriteEndElement();
    std::string out = w->ToString();
    EXPECT_NE(out.find("<name>Alice</name>"), std::string::npos);
}

TEST(XmlWriterTests, WriteStartDocument_AddsDeclaration) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    w->WriteStartDocument();
    w->WriteStartElement("root");
    w->WriteEndElement();
    std::string out = w->ToString();
    EXPECT_NE(out.find("<?xml"), std::string::npos);
}

TEST(XmlWriterTests, WriteComment_InOutput) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    w->WriteStartElement("root");
    w->WriteComment("a comment");
    w->WriteEndElement();
    EXPECT_NE(w->ToString().find("a comment"), std::string::npos);
}

TEST(XmlWriterTests, NestedElements_InOutput) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    w->WriteStartElement("root");
    w->WriteStartElement("child");
    w->WriteEndElement();
    w->WriteEndElement();
    std::string out = w->ToString();
    EXPECT_NE(out.find("<child"), std::string::npos);
    EXPECT_NE(out.find("</root>"), std::string::npos);
}

TEST(XmlWriterTests, Flush_DoesNotThrow) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    EXPECT_NO_THROW(w->Flush());
}

TEST(XmlWriterTests, Close_DoesNotThrow) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    w->WriteStartElement("root");
    w->WriteEndElement();
    EXPECT_NO_THROW(w->Close());
}

// ===========================================================================
// Round-trip: write then read back
// ===========================================================================

TEST(XmlRoundTripTests, WriteAndReadBack) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    w->WriteStartElement("data");
    w->WriteAttributeString("version", "2");
    w->WriteElementString("item", "value1");
    w->WriteEndElement();
    std::string xml = w->ToString();

    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString(xml));
    r->Read(); // <data>
    EXPECT_EQ(r->getNameProperty(), "data");
    EXPECT_EQ(r->GetAttribute("version"), "2");
    r->Read(); // <item>
    EXPECT_EQ(r->getNameProperty(), "item");
    std::string text = r->ReadElementContentAsString();
    EXPECT_EQ(text, "value1");
}

// ===========================================================================
// XName
// ===========================================================================

TEST(XNameTests, DefaultConstructor_EmptyNames) {
    XName n;
    EXPECT_EQ(n.getLocalNameProperty(), "");
    EXPECT_EQ(n.getNamespaceNameProperty(), "");
}

TEST(XNameTests, LocalNameConstructor) {
    XName n("item");
    EXPECT_EQ(n.getLocalNameProperty(), "item");
    EXPECT_EQ(n.getNamespaceNameProperty(), "");
}

TEST(XNameTests, FullConstructor_StoresBoth) {
    XName n("http://example.com", "item");
    EXPECT_EQ(n.getNamespaceNameProperty(), "http://example.com");
    EXPECT_EQ(n.getLocalNameProperty(), "item");
}

TEST(XNameTests, ToString_LocalOnly) {
    XName n("item");
    EXPECT_EQ(n.ToString(), "item");
}

TEST(XNameTests, ToString_WithNamespace) {
    XName n("http://ex.com", "item");
    EXPECT_EQ(n.ToString(), "{http://ex.com}item");
}

TEST(XNameTests, Equality_SameLocalName) {
    EXPECT_TRUE(XName("a") == XName("a"));
}

TEST(XNameTests, Equality_DifferentNames) {
    EXPECT_FALSE(XName("a") == XName("b"));
}

TEST(XNameTests, Inequality) {
    EXPECT_TRUE(XName("a") != XName("b"));
}

TEST(XNameTests, Get_LocalOnly) {
    XName n = XName::Get("root");
    EXPECT_EQ(n.getLocalNameProperty(), "root");
    EXPECT_EQ(n.getNamespaceNameProperty(), "");
}

TEST(XNameTests, Get_WithNamespace) {
    XName n = XName::Get("{http://ex.com}root");
    EXPECT_EQ(n.getNamespaceNameProperty(), "http://ex.com");
    EXPECT_EQ(n.getLocalNameProperty(), "root");
}

// ===========================================================================
// XAttribute
// ===========================================================================

TEST(XAttributeTests, Constructor_StringName) {
    XAttribute a("id", "42");
    EXPECT_EQ(a.getNameProperty().getLocalNameProperty(), "id");
    EXPECT_EQ(a.getValueProperty(), "42");
}

TEST(XAttributeTests, Constructor_XName) {
    XAttribute a(XName("class"), "main");
    EXPECT_EQ(a.getValueProperty(), "main");
}

TEST(XAttributeTests, SetValue) {
    XAttribute a("x", "old");
    a.setValueProperty("new");
    EXPECT_EQ(a.getValueProperty(), "new");
}

TEST(XAttributeTests, ToString_LocalName) {
    XAttribute a("href", "url");
    EXPECT_EQ(a.ToString(), "href=\"url\"");
}

TEST(XAttributeTests, ToString_NamespacedAttribute_DoesNotEmitClarkNotation) {
    // Regression: XAttribute::ToString() previously wrote XName::ToString()'s Clark notation
    // ("{namespace}local") directly as the attribute *name* -- '{'/'}' are not legal in an XML
    // Name production, so this produced literally malformed, unparseable XML for any
    // namespaced attribute instead of just a namespace-fidelity gap.
    XAttribute a(XName("http://example.com/ns", "kind"), "rare");
    std::string s = a.ToString();
    EXPECT_EQ(s.find('{'), std::string::npos);
    EXPECT_EQ(s.find('}'), std::string::npos);
    EXPECT_EQ(s, "kind=\"rare\"");
}

TEST(XAttributeTests, NextAttribute_DefaultNull) {
    XAttribute a("x", "1");
    EXPECT_EQ(a.getNextAttributeProperty(), nullptr);
}

// ===========================================================================
// XElement
// ===========================================================================

TEST(XElementTests, Constructor_StringName) {
    XElement e("item");
    EXPECT_EQ(e.getNameProperty().getLocalNameProperty(), "item");
}

TEST(XElementTests, Constructor_NameAndValue) {
    XElement e(XName("price"), "9.99");
    EXPECT_EQ(e.getValueProperty(), "9.99");
}

TEST(XElementTests, SetValue) {
    XElement e("x");
    e.setValueProperty("hello");
    EXPECT_EQ(e.getValueProperty(), "hello");
}

TEST(XElementTests, AddAndFindAttribute) {
    XElement e("div");
    e.Add(std::make_shared<XAttribute>("class", "box"));
    auto attr = e.Attribute("class");
    ASSERT_NE(attr, nullptr);
    EXPECT_EQ(attr->getValueProperty(), "box");
}

TEST(XElementTests, Attribute_NotFound_ReturnsNull) {
    XElement e("div");
    EXPECT_EQ(e.Attribute("missing"), nullptr);
}

TEST(XElementTests, AddAndFindChildElement) {
    XElement root("root");
    root.Add(std::make_shared<XElement>("child"));
    auto child = root.Element("child");
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->getNameProperty().getLocalNameProperty(), "child");
}

TEST(XElementTests, Element_NotFound_ReturnsNull) {
    XElement e("root");
    EXPECT_EQ(e.Element("missing"), nullptr);
}

TEST(XElementTests, Elements_ReturnsAllChildren) {
    XElement root("root");
    root.Add(std::make_shared<XElement>("a"));
    root.Add(std::make_shared<XElement>("b"));
    EXPECT_EQ(root.Elements().size(), 2u);
}

TEST(XElementTests, Elements_ByName_FiltersCorrectly) {
    XElement root("root");
    root.Add(std::make_shared<XElement>("a"));
    root.Add(std::make_shared<XElement>("b"));
    root.Add(std::make_shared<XElement>("a"));
    EXPECT_EQ(root.Elements("a").size(), 2u);
}

TEST(XElementTests, Descendants_FindsNested) {
    XElement root("root");
    auto child = std::make_shared<XElement>("child");
    auto nested = std::make_shared<XElement>("target");
    child->Add(nested);
    root.Add(child);
    auto found = root.Descendants(XName("target"));
    EXPECT_EQ(found.size(), 1u);
}

TEST(XElementTests, GetAttributeValue_Present) {
    XElement e("el");
    e.Add(std::make_shared<XAttribute>("key", "val"));
    auto v = e.getAttributeValue("key");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "val");
}

TEST(XElementTests, GetAttributeValue_Absent_NullOpt) {
    XElement e("el");
    EXPECT_FALSE(e.getAttributeValue("missing").has_value());
}

TEST(XElementTests, ToString_SelfClosing) {
    XElement e("br");
    EXPECT_EQ(e.ToString(), "<br/>");
}

TEST(XElementTests, ToString_WithValue) {
    XElement e(XName("name"), "Alice");
    EXPECT_EQ(e.ToString(), "<name>Alice</name>");
}

TEST(XElementTests, ToString_WithAttribute) {
    XElement e("img");
    e.Add(std::make_shared<XAttribute>("src", "pic.png"));
    std::string s = e.ToString();
    EXPECT_NE(s.find("src=\"pic.png\""), std::string::npos);
}

TEST(XElementTests, ToString_NamespacedAttribute_ProducesValidXml_RoundTrips) {
    // Regression: previously ToString() (via SerializeTo -> XAttribute::ToString()) emitted
    // "{http://example.com/ns}kind=\"rare\"", which is not valid XML and would fail to
    // reparse. Confirms the fixed output both omits Clark notation and successfully
    // round-trips through Parse().
    XElement e("special");
    e.Add(std::make_shared<XAttribute>(XName("http://example.com/ns", "kind"), "rare"));
    std::string s = e.ToString();
    EXPECT_EQ(s.find('{'), std::string::npos);

    auto reloaded = XElement::Parse(s);
    ASSERT_NE(reloaded, nullptr);
    EXPECT_EQ(reloaded->Attribute("kind")->getValueProperty(), "rare");
}

// ===========================================================================
// XDeclaration + XDocument
// ===========================================================================

TEST(XDeclarationTests, Constructor_StoresFields) {
    XDeclaration decl("1.0", "utf-8", "yes");
    EXPECT_EQ(decl.getVersionProperty(), "1.0");
    EXPECT_EQ(decl.getEncodingProperty(), "utf-8");
    EXPECT_EQ(decl.getStandaloneProperty(), "yes");
}

TEST(XDeclarationTests, ToString_ContainsVersion) {
    XDeclaration decl("1.0", "utf-8", "no");
    EXPECT_NE(decl.ToString().find("1.0"), std::string::npos);
}

TEST(XDocumentTests, DefaultConstructor_NullRoot) {
    XDocument doc;
    EXPECT_EQ(doc.getRootProperty(), nullptr);
}

TEST(XDocumentTests, Constructor_WithRoot) {
    auto root = std::make_shared<XElement>("root");
    XDocument doc(root);
    ASSERT_NE(doc.getRootProperty(), nullptr);
    EXPECT_EQ(doc.getRootProperty()->getNameProperty().getLocalNameProperty(), "root");
}

TEST(XDocumentTests, SetRoot) {
    XDocument doc;
    doc.setRootProperty(std::make_shared<XElement>("x"));
    ASSERT_NE(doc.getRootProperty(), nullptr);
}

TEST(XDocumentTests, SetDeclaration) {
    XDocument doc;
    doc.setDeclarationProperty(std::make_shared<XDeclaration>("1.0", "utf-8", "yes"));
    ASSERT_NE(doc.getDeclarationProperty(), nullptr);
    EXPECT_EQ(doc.getDeclarationProperty()->getVersionProperty(), "1.0");
}

TEST(XDocumentTests, Element_MatchesRootByName) {
    auto root = std::make_shared<XElement>("catalog");
    XDocument doc(root);
    EXPECT_NE(doc.Element("catalog"), nullptr);
    EXPECT_EQ(doc.Element("other"), nullptr);
}

TEST(XDocumentTests, ToString_ContainsRootTag) {
    auto root = std::make_shared<XElement>("data");
    XDocument doc(root);
    EXPECT_NE(doc.ToString().find("<data"), std::string::npos);
}

TEST(XDocumentTests, Parse_ReturnsNonNull) {
    auto doc = XDocument::Parse("<root/>");
    EXPECT_NE(doc, nullptr);
}

TEST(XDocumentTests, Parse_ParsesRealContent) {
    auto doc = XDocument::Parse("<catalog><item id=\"1\">Widget</item></catalog>");
    ASSERT_NE(doc->getRootProperty(), nullptr);
    EXPECT_EQ(doc->getRootProperty()->getNameProperty().getLocalNameProperty(), "catalog");
    auto item = doc->getRootProperty()->Element("item");
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->getAttributeValue("id"), "1");
    EXPECT_EQ(item->getValueProperty(), "Widget");
}

TEST(XDocumentTests, Parse_MalformedXml_Throws) {
    EXPECT_THROW(XDocument::Parse("<unclosed>"), XmlException);
}

TEST(XDocumentTests, Load_MissingFile_Throws) {
    EXPECT_THROW(XDocument::Load("nonexistent.xml"), XmlException);
}

TEST(XDocumentTests, Load_ExistingFile_ReturnsParsedDocument) {
    const char* path = "xdocument_load_test.xml";
    {
        std::ofstream ofs(path);
        ofs << "<root><child>value</child></root>";
    }
    auto doc = XDocument::Load(path);
    ASSERT_NE(doc->getRootProperty(), nullptr);
    EXPECT_EQ(doc->getRootProperty()->getNameProperty().getLocalNameProperty(), "root");
    std::remove(path);
}
