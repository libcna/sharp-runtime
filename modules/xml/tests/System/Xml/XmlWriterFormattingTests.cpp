// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// XmlWriter text form. The writer used to hand its DOM to tinyxml2's XMLPrinter, whose output
// differed from .NET's XmlWriter in five measured ways: a fixed four-space indent regardless
// of XmlWriterSettings::IndentChars, "<a/>" for an empty element where .NET writes "<a />",
// encoding="UTF-8" in the declaration where .NET writes "utf-8", a trailing newline .NET does
// not write, and indented children after text had been written into an element, where .NET
// stops indenting for the rest of that element (its mixed-content rule). These tests pin the
// .NET form, which the XNA content pipeline's IntermediateSerializer output (CNA
// tests/reference/xna40/intermediate/) exhibits verbatim.
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "System/Xml/NewLineHandling.hpp"
#include "System/Xml/XmlReader.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "System/Xml/XmlWriterSettings.hpp"

using System::Xml::NewLineHandling;
using System::Xml::XmlReader;
using System::Xml::XmlWriter;
using System::Xml::XmlWriterSettings;

namespace {

XmlWriterSettings Indented() {
    XmlWriterSettings settings;
    settings.Indent = true;
    settings.NewLineChars = "\n";
    return settings;
}

TEST(XmlWriterFormattingTests, IndentedDocumentMatchesDotNetSpelling) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString(Indented()));
    w->WriteStartDocument();
    w->WriteStartElement("XnaContent");
    w->WriteAttributeString("xmlns:Framework", "Microsoft.Xna.Framework");
    w->WriteStartElement("Asset");
    w->WriteAttributeString("Type", "Framework:Vector3");
    w->WriteString("1 2 3");
    w->WriteEndElement();
    w->WriteStartElement("Empty");
    w->WriteEndElement();
    w->WriteStartElement("EmptyString");
    w->WriteString("");
    w->WriteEndElement();
    w->WriteStartElement("Nested");
    w->WriteElementString("Name", "a<b&c \"q\" >d");
    w->WriteEndElement();
    w->WriteEndElement();
    w->WriteEndDocument();
    EXPECT_EQ(w->ToString(),
              "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
              "<XnaContent xmlns:Framework=\"Microsoft.Xna.Framework\">\n"
              "  <Asset Type=\"Framework:Vector3\">1 2 3</Asset>\n"
              "  <Empty />\n"
              "  <EmptyString></EmptyString>\n"
              "  <Nested>\n"
              "    <Name>a&lt;b&amp;c \"q\" &gt;d</Name>\n"
              "  </Nested>\n"
              "</XnaContent>");
}

TEST(XmlWriterFormattingTests, MixedContentStopsIndentingForTheRestOfTheElement) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString(Indented()));
    w->WriteStartElement("Asset");
    w->WriteElementString("Value", "7");
    w->WriteString("1 2");
    w->WriteElementString("RenamedItems", "3 4");
    w->WriteString("5 6");
    w->WriteStartElement("Named");
    w->WriteElementString("Inner", "x");
    w->WriteEndElement();
    w->WriteEndElement();
    EXPECT_EQ(w->ToString(),
              "<Asset>\n"
              "  <Value>7</Value>1 2<RenamedItems>3 4</RenamedItems>5 6<Named>\n"
              "    <Inner>x</Inner>\n"
              "  </Named></Asset>");
}

TEST(XmlWriterFormattingTests, IndentCharsAndNewLineCharsAreHonoured) {
    XmlWriterSettings settings;
    settings.Indent = true;
    settings.IndentChars = "\t";
    settings.NewLineChars = "\r\n";
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString(settings));
    w->WriteStartElement("a");
    w->WriteStartElement("b");
    w->WriteEndElement();
    w->WriteEndElement();
    EXPECT_EQ(w->ToString(), "<a>\r\n\t<b />\r\n</a>");
}

TEST(XmlWriterFormattingTests, CompactOutputHasNoWhitespaceAndNoTrailingNewline) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    w->WriteStartDocument();
    w->WriteStartElement("a");
    w->WriteStartElement("b");
    w->WriteEndElement();
    w->WriteEndElement();
    EXPECT_EQ(w->ToString(), "<?xml version=\"1.0\" encoding=\"utf-8\"?><a><b /></a>");
}

TEST(XmlWriterFormattingTests, OmitXmlDeclarationDropsIt) {
    XmlWriterSettings settings;
    settings.OmitXmlDeclaration = true;
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString(settings));
    w->WriteStartDocument();
    w->WriteStartElement("a");
    w->WriteEndElement();
    EXPECT_EQ(w->ToString(), "<a />");
}

TEST(XmlWriterFormattingTests, TextNewLinesFollowNewLineHandling) {
    XmlWriterSettings replace;
    replace.NewLineChars = "\r\n";
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString(replace));
    w->WriteStartElement("s");
    w->WriteString("line1\nline2\r\nline3\rline4");
    w->WriteEndElement();
    EXPECT_EQ(w->ToString(), "<s>line1\r\nline2\r\nline3\r\nline4</s>");

    XmlWriterSettings entitize;
    entitize.NewLineHandling = NewLineHandling::Entitize;
    std::unique_ptr<XmlWriter> e(XmlWriter::CreateToString(entitize));
    e->WriteStartElement("s");
    e->WriteString("a\r\nb");
    e->WriteEndElement();
    EXPECT_EQ(e->ToString(), "<s>a&#xD;\nb</s>");

    XmlWriterSettings none;
    none.NewLineHandling = NewLineHandling::None;
    std::unique_ptr<XmlWriter> n(XmlWriter::CreateToString(none));
    n->WriteStartElement("s");
    n->WriteString("a\r\nb");
    n->WriteEndElement();
    EXPECT_EQ(n->ToString(), "<s>a\r\nb</s>");
}

TEST(XmlWriterFormattingTests, AttributeValuesEscapeMarkupAndWhitespaceControls) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
    w->WriteStartElement("a");
    w->WriteAttributeString("v", "x<y>&\"q\"\tz\r\n");
    w->WriteEndElement();
    EXPECT_EQ(w->ToString(), "<a v=\"x&lt;y&gt;&amp;&quot;q&quot;&#x9;z&#xD;&#xA;\" />");
}

TEST(XmlWriterFormattingTests, CommentsCdataAndProcessingInstructionsAreIndentedLikeElements) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString(Indented()));
    w->WriteStartElement("r");
    w->WriteComment(" c ");
    w->WriteProcessingInstruction("pi", "data");
    w->WriteStartElement("t");
    w->WriteCData("a<b");
    w->WriteEndElement();
    w->WriteEndElement();
    EXPECT_EQ(w->ToString(),
              "<r>\n"
              "  <!-- c -->\n"
              "  <?pi data?>\n"
              "  <t><![CDATA[a<b]]></t>\n"
              "</r>");
}

TEST(XmlWriterFormattingTests, OutputReadsBackThroughXmlReader) {
    std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString(Indented()));
    w->WriteStartDocument();
    w->WriteStartElement("r");
    w->WriteElementString("t", "a<b&c");
    w->WriteEndElement();
    std::unique_ptr<XmlReader> r(XmlReader::CreateFromString(w->ToString()));
    r->ReadStartElement("r");
    ASSERT_TRUE(r->IsStartElement("t"));
    EXPECT_EQ(r->ReadElementContentAsString(), "a<b&c");
}

} // namespace
