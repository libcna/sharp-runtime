// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2076 / SR-AUD-349 — XmlWriter name and lifecycle validation.
//
// SR-AUD-349 recorded that the writer "emits malformed XML because names and writer state
// are not validated": WriteStartElement("1bad") produced "<1bad/>", which this module's OWN
// XmlReader then rejected with XML_ERROR_PARSING. Measured against 1b65f0f
// (build-probe/2076_probe1_before.log), the surface was wider than the finding's one example:
//
//   * FOUR public doors take an XML name and validated none of them — WriteStartElement,
//     WriteAttributeString, WriteProcessingInstruction (the PI *target*) and WriteDocType
//     (the DOCTYPE root-element name). WriteElementString inherits the first.
//   * EVERY Write* member stayed callable after Close(), silently discarding its argument.
//   * An unbalanced WriteEndElement(), and an attribute written with no element open, were
//     also silently discarded.
//
// The repair routes every name door through XmlConvert::VerifyName — the validator this
// module already ships and that XmlDocument::CreateElement already uses — so the writer door
// and the DOM door now report an identical diagnostic for identical input. It invents no name
// grammar. See docs/SystemXmlNamespaceReviewPlan.md §4.3 and §20.4.
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "System/ArgumentException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Xml/XmlDocument.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlReader.hpp"
#include "System/Xml/XmlTextWriter.hpp"
#include "System/Xml/XmlWriter.hpp"

using System::ArgumentException;
using System::InvalidOperationException;
using System::Xml::XmlDocument;
using System::Xml::XmlException;
using System::Xml::XmlReader;
using System::Xml::XmlTextWriter;
using System::Xml::XmlWriter;

namespace {

std::unique_ptr<XmlWriter> NewWriter() {
    return std::unique_ptr<XmlWriter>(XmlWriter::CreateToString());
}

/// The closure property SR-AUD-349 is really about: whatever this writer emits, this
/// module's own reader must be able to consume.
void ExpectReReadable(const std::string& xml) {
    ASSERT_FALSE(xml.empty());
    std::unique_ptr<XmlReader> reader;
    ASSERT_NO_THROW(reader.reset(XmlReader::CreateFromString(xml))) << "not re-readable: " << xml;
    EXPECT_NE(reader, nullptr);
}

} // namespace

// ===========================================================================
// #2076 — invalid element names
// ===========================================================================

TEST(XmlWriterValidationTests, WriteStartElement_InvalidName_ThrowsXmlExceptionNamingTheName) {
    auto w = NewWriter();
    try {
        w->WriteStartElement("1bad");
        FAIL() << "expected XmlException";
    } catch (const XmlException& e) {
        EXPECT_NE(e.getMessageProperty().find("1bad"), std::string::npos) << e.getMessageProperty();
    }
}

TEST(XmlWriterValidationTests, WriteStartElement_EveryMeasuredInvalidName_Throws) {
    // Exactly the names build-probe/2076_probe1_before.log measured as accepted-and-unreadable.
    for (const char* name : {"1bad", "a b", "a<b", "a>b", "a\"b", "a&b", "a\tb", "a\nb",
                             "-lead", ".lead", ":lead", "a b\r\nc"}) {
        auto w = NewWriter();
        EXPECT_THROW(w->WriteStartElement(name), XmlException) << "name: " << name;
    }
}

TEST(XmlWriterValidationTests, WriteStartElement_EmptyName_ThrowsArgumentException) {
    // XmlConvert::VerifyName's own pre-existing choice for an empty argument, deliberately
    // not re-mapped: the writer door and XmlDocument::CreateElement("") now agree exactly.
    auto w = NewWriter();
    EXPECT_THROW(w->WriteStartElement(""), ArgumentException);
    XmlDocument doc;
    EXPECT_THROW((void)doc.CreateElement(""), ArgumentException);
}

TEST(XmlWriterValidationTests, WriteStartElement_ValidNames_StillAccepted_AndReReadable) {
    for (const char* name : {"a", "A", "_a", "a1", "a.b", "a-b", "a_b", "ns:local", "a:b:c",
                             "\xc3\xa9l\xc3\xa9ment"}) {
        auto w = NewWriter();
        ASSERT_NO_THROW(w->WriteStartElement(name)) << "name: " << name;
        ASSERT_NO_THROW(w->WriteEndElement());
        ExpectReReadable(w->ToString());
    }
}

TEST(XmlWriterValidationTests, WriteStartElement_RejectedNameLeavesTheDocumentUntouched) {
    auto w = NewWriter();
    w->WriteStartElement("root");
    w->WriteEndElement();
    const std::string before = w->ToString();
    EXPECT_THROW(w->WriteStartElement("1bad"), XmlException);
    EXPECT_EQ(w->ToString(), before); // no partial element was published
}

// ===========================================================================
// #2076 — invalid attribute names
// ===========================================================================

TEST(XmlWriterValidationTests, WriteAttributeString_InvalidName_Throws) {
    for (const char* name : {"1bad", "a b", "a<b", "a=b", "a\"b"}) {
        auto w = NewWriter();
        w->WriteStartElement("e");
        EXPECT_THROW(w->WriteAttributeString(name, "v"), XmlException) << "name: " << name;
    }
}

TEST(XmlWriterValidationTests, WriteAttributeString_EmptyName_ThrowsArgumentException) {
    auto w = NewWriter();
    w->WriteStartElement("e");
    EXPECT_THROW(w->WriteAttributeString("", "v"), ArgumentException);
}

TEST(XmlWriterValidationTests, WriteAttributeString_ValidName_AcceptedAndReReadable) {
    auto w = NewWriter();
    w->WriteStartElement("e");
    ASSERT_NO_THROW(w->WriteAttributeString("ns:attr-1.0_x", "v"));
    w->WriteEndElement();
    ExpectReReadable(w->ToString());
}

TEST(XmlWriterValidationTests, WriteAttributeString_RejectedNameLeavesTheElementUntouched) {
    auto w = NewWriter();
    w->WriteStartElement("e");
    w->WriteAttributeString("good", "1");
    EXPECT_THROW(w->WriteAttributeString("1bad", "2"), XmlException);
    w->WriteEndElement();
    EXPECT_EQ(w->ToString(), "<e good=\"1\"/>");
}

TEST(XmlWriterValidationTests, WriteElementString_InvalidName_ThrowsAndWritesNothing) {
    auto w = NewWriter();
    EXPECT_THROW(w->WriteElementString("1bad", "v"), XmlException);
    EXPECT_EQ(w->ToString(), "");
}

// ===========================================================================
// #2076 — the two name doors SR-AUD-349 does not enumerate
// ===========================================================================

TEST(XmlWriterValidationTests, WriteProcessingInstruction_InvalidTarget_Throws) {
    for (const char* target : {"1bad", "a b", "a?>b"}) {
        auto w = NewWriter();
        EXPECT_THROW(w->WriteProcessingInstruction(target, "d"), XmlException) << target;
    }
    auto w = NewWriter();
    EXPECT_THROW(w->WriteProcessingInstruction("", "d"), ArgumentException);
}

TEST(XmlWriterValidationTests, WriteProcessingInstruction_TargetWithCloseSequence_NoLongerSpills) {
    // Before the repair this emitted "<?a?>b d?><r/>": the "?>" inside the TARGET closed the
    // instruction early and spilled "b d?>" into document-level text. The data was already
    // sanitized; the target had no door at all.
    auto w = NewWriter();
    EXPECT_THROW(w->WriteProcessingInstruction("a?>b", "d"), XmlException);
    w->WriteStartElement("r");
    w->WriteEndElement();
    EXPECT_EQ(w->ToString(), "<r/>");
}

TEST(XmlWriterValidationTests, WriteProcessingInstruction_ValidTarget_StillAccepted) {
    auto w = NewWriter();
    ASSERT_NO_THROW(w->WriteProcessingInstruction("style-sheet", "type=\"text/xsl\""));
    w->WriteStartElement("r");
    w->WriteEndElement();
    ExpectReReadable(w->ToString());
}

TEST(XmlWriterValidationTests, WriteDocType_InvalidName_Throws) {
    for (const char* name : {"1bad", "a b"}) {
        auto w = NewWriter();
        EXPECT_THROW(w->WriteDocType(name, "", "", ""), XmlException) << name;
    }
    auto w = NewWriter();
    EXPECT_THROW(w->WriteDocType("", "", "", ""), ArgumentException);
}

TEST(XmlWriterValidationTests, WriteDocType_ValidName_StillAccepted) {
    auto w = NewWriter();
    ASSERT_NO_THROW(w->WriteDocType("html", "", "about:legacy-compat", ""));
    w->WriteStartElement("html");
    w->WriteEndElement();
    ExpectReReadable(w->ToString());
}

// ===========================================================================
// #2076 — writer state: unbalanced WriteEndElement (cause X-D)
// ===========================================================================

TEST(XmlWriterValidationTests, WriteEndElement_WithNothingOpen_ThrowsInvalidOperation) {
    auto w = NewWriter();
    EXPECT_THROW(w->WriteEndElement(), InvalidOperationException);
}

TEST(XmlWriterValidationTests, WriteEndElement_OneTooMany_ThrowsInvalidOperation) {
    auto w = NewWriter();
    w->WriteStartElement("e");
    w->WriteEndElement();
    EXPECT_THROW(w->WriteEndElement(), InvalidOperationException);
    EXPECT_EQ(w->ToString(), "<e/>"); // the balanced part survived intact
}

TEST(XmlWriterValidationTests, WriteEndElement_BalancedNesting_StillAccepted) {
    auto w = NewWriter();
    w->WriteStartElement("a");
    w->WriteStartElement("b");
    w->WriteEndElement();
    w->WriteEndElement();
    EXPECT_THROW(w->WriteEndElement(), InvalidOperationException);
    EXPECT_EQ(w->ToString(), "<a><b/></a>");
}

TEST(XmlWriterValidationTests, WriteAttributeString_WithNoElementOpen_ThrowsInvalidOperation) {
    auto w = NewWriter();
    EXPECT_THROW(w->WriteAttributeString("a", "v"), InvalidOperationException);
    // ...and after the only element has been closed again.
    w->WriteStartElement("e");
    w->WriteEndElement();
    EXPECT_THROW(w->WriteAttributeString("a", "v"), InvalidOperationException);
}

TEST(XmlWriterValidationTests, WriteAttributeString_NameCheckedBeforeStateCheck) {
    // Argument validation precedes state validation, so a caller who got BOTH wrong is told
    // about the argument they control rather than about the writer's cursor.
    auto w = NewWriter();
    EXPECT_THROW(w->WriteAttributeString("1bad", "v"), XmlException);
}

// ===========================================================================
// #2076 — writer state: after Close() (cause X-D)
// ===========================================================================

TEST(XmlWriterValidationTests, EveryWriteAfterClose_ThrowsInvalidOperation) {
    const auto closedWriter = [] {
        auto w = NewWriter();
        w->WriteStartElement("e");
        w->WriteEndElement();
        w->Close();
        return w;
    };
    EXPECT_THROW(closedWriter()->WriteStartDocument(), InvalidOperationException);
    EXPECT_THROW(closedWriter()->WriteEndDocument(), InvalidOperationException);
    EXPECT_THROW(closedWriter()->WriteStartElement("x"), InvalidOperationException);
    EXPECT_THROW(closedWriter()->WriteEndElement(), InvalidOperationException);
    EXPECT_THROW(closedWriter()->WriteAttributeString("a", "v"), InvalidOperationException);
    EXPECT_THROW(closedWriter()->WriteString("s"), InvalidOperationException);
    EXPECT_THROW(closedWriter()->WriteWhitespace(" "), InvalidOperationException);
    EXPECT_THROW(closedWriter()->WriteElementString("x", "v"), InvalidOperationException);
    EXPECT_THROW(closedWriter()->WriteComment("c"), InvalidOperationException);
    EXPECT_THROW(closedWriter()->WriteCData("c"), InvalidOperationException);
    EXPECT_THROW(closedWriter()->WriteProcessingInstruction("t", "d"), InvalidOperationException);
    EXPECT_THROW(closedWriter()->WriteDocType("r", "", "", ""), InvalidOperationException);
}

TEST(XmlWriterValidationTests, WriteAfterClose_DoesNotChangeTheEmittedDocument) {
    auto w = NewWriter();
    w->WriteStartElement("e");
    w->WriteEndElement();
    w->Close();
    const std::string after = w->ToString();
    EXPECT_THROW(w->WriteStartElement("late"), InvalidOperationException);
    EXPECT_EQ(w->ToString(), after);
    EXPECT_EQ(after, "<e/>");
}

TEST(XmlWriterValidationTests, CloseIsIdempotentAndToStringStaysUsable) {
    // Close() must stay callable twice: the destructor calls it unconditionally, and
    // ToString() is the only way to read an in-memory writer's result back.
    auto w = NewWriter();
    w->WriteStartElement("e");
    w->WriteEndElement();
    ASSERT_NO_THROW(w->Close());
    ASSERT_NO_THROW(w->Close());
    ASSERT_NO_THROW(w->Flush());
    EXPECT_EQ(w->ToString(), "<e/>");
}

TEST(XmlWriterValidationTests, CloseAfterAFailedCloseDoesNotThrowAgain) {
    // Close() marks the writer closed BEFORE flushing, so a writer whose save failed is
    // terminally closed. That is what makes ~XmlWriter()'s unconditional Close() safe: it
    // must not re-attempt a save that already failed. (This is the assertion that makes the
    // idempotent early-return load-bearing — without it the second Close() throws again.)
    std::unique_ptr<XmlWriter> w(XmlWriter::Create("/nonexistent-dir-for-2076-test/out.xml"));
    w->WriteStartElement("root");
    w->WriteEndElement();
    EXPECT_THROW(w->Close(), XmlException);
    EXPECT_NO_THROW(w->Close());
    // ...and the writer is closed, not merely un-saved.
    EXPECT_THROW(w->WriteStartElement("late"), InvalidOperationException);
}

TEST(XmlWriterValidationTests, XmlTextWriter_InheritsBothRepairs) {
    // XmlTextWriter forwards to an owned XmlWriter, so it must gain both halves for free.
    XmlTextWriter bad;
    EXPECT_THROW(bad.WriteStartElement("1bad"), XmlException);

    XmlTextWriter closed;
    closed.WriteStartElement("e");
    closed.WriteEndElement();
    closed.Close();
    EXPECT_THROW(closed.WriteStartElement("late"), InvalidOperationException);
    EXPECT_EQ(closed.ToString(), "<e/>");
}

// ===========================================================================
// #2076 — the closure property, stated directly
// ===========================================================================

TEST(XmlWriterValidationTests, EveryAcceptedNameProducesAReReadableDocument) {
    // The finding's own sentence, as a property: for every name the writer now accepts,
    // this module's reader consumes the result.
    for (const char* name : {"a", "A", "_a", "a1", "a.b", "a-b", "a_b", "ns:local", "a:b:c"}) {
        auto w = NewWriter();
        w->WriteStartElement(name);
        w->WriteAttributeString(name, "v");
        w->WriteString("text");
        w->WriteEndElement();
        ExpectReReadable(w->ToString());
    }
}

TEST(XmlWriterValidationTests, WriterAndDomAgreeOnEveryProbedName) {
    // The corrected premise of docs/SystemXmlNamespaceReviewPlan.md §6.2 as an assertion:
    // the writer and XmlDocument::CreateElement route through one validator, so they can no
    // longer disagree about whether a name is legal.
    for (const char* name : {"1bad", "a b", "a<b", "-lead", ":lead", "a", "_a", "ns:local", "a:b:c"}) {
        bool writerThrew = false;
        bool domThrew = false;
        try {
            auto w = NewWriter();
            w->WriteStartElement(name);
        } catch (const XmlException&) { writerThrew = true; }
        try {
            XmlDocument doc;
            (void)doc.CreateElement(name);
        } catch (const XmlException&) { domThrew = true; }
        EXPECT_EQ(writerThrew, domThrew) << "name: " << name;
    }
}
