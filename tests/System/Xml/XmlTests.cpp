// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Note: XmlReader::Create, XmlWriter::Create and most virtual methods are stubs that
// throw NotImplementedException (awaiting tinyxml2/pugixml integration).
// XDocument::Save is declared but not yet defined — not tested here.
// System::Xml::Linq types (XName, XAttribute, XElement, XDocument) are fully implemented.
#include <gtest/gtest.h>
#include "System/NotImplementedException.hpp"
#include "System/Xml/XmlReader.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "System/Xml/Linq/XName.hpp"
#include "System/Xml/Linq/XAttribute.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XDocument.hpp"

using System::NotImplementedException;
using System::Xml::ReadState;
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
// XmlReader stub — throws, except Close()
// ===========================================================================

struct ConcreteXmlReader : XmlReader {};

TEST(XmlReaderTests, Read_ThrowsNotImplemented) {
    ConcreteXmlReader r;
    EXPECT_THROW(r.Read(), NotImplementedException);
}

TEST(XmlReaderTests, GetNodeType_ThrowsNotImplemented) {
    ConcreteXmlReader r;
    EXPECT_THROW((void)r.getNodeTypeProperty(), NotImplementedException);
}

TEST(XmlReaderTests, GetName_ThrowsNotImplemented) {
    ConcreteXmlReader r;
    EXPECT_THROW((void)r.getNameProperty(), NotImplementedException);
}

TEST(XmlReaderTests, GetAttribute_ThrowsNotImplemented) {
    ConcreteXmlReader r;
    EXPECT_THROW((void)r.GetAttribute("attr"), NotImplementedException);
}

TEST(XmlReaderTests, Close_DoesNotThrow) {
    ConcreteXmlReader r;
    EXPECT_NO_THROW(r.Close());
}

TEST(XmlReaderTests, Create_ThrowsNotImplemented) {
    EXPECT_THROW(XmlReader::Create("file.xml"), NotImplementedException);
}

// ===========================================================================
// XmlWriter stub — throws, except Flush() and Close()
// ===========================================================================

struct ConcreteXmlWriter : XmlWriter {};

TEST(XmlWriterTests, WriteStartDocument_ThrowsNotImplemented) {
    ConcreteXmlWriter w;
    EXPECT_THROW(w.WriteStartDocument(), NotImplementedException);
}

TEST(XmlWriterTests, WriteStartElement_ThrowsNotImplemented) {
    ConcreteXmlWriter w;
    EXPECT_THROW(w.WriteStartElement("root"), NotImplementedException);
}

TEST(XmlWriterTests, WriteAttributeString_ThrowsNotImplemented) {
    ConcreteXmlWriter w;
    EXPECT_THROW(w.WriteAttributeString("key", "value"), NotImplementedException);
}

TEST(XmlWriterTests, WriteString_ThrowsNotImplemented) {
    ConcreteXmlWriter w;
    EXPECT_THROW(w.WriteString("text"), NotImplementedException);
}

TEST(XmlWriterTests, Flush_DoesNotThrow) {
    ConcreteXmlWriter w;
    EXPECT_NO_THROW(w.Flush());
}

TEST(XmlWriterTests, Close_DoesNotThrow) {
    ConcreteXmlWriter w;
    EXPECT_NO_THROW(w.Close());
}

TEST(XmlWriterTests, Create_ThrowsNotImplemented) {
    EXPECT_THROW(XmlWriter::Create("out.xml"), NotImplementedException);
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

TEST(XDocumentTests, Load_ReturnsNonNull) {
    auto doc = XDocument::Load("nonexistent.xml");
    EXPECT_NE(doc, nullptr);
}
