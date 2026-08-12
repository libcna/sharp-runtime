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
#include "System/Xml/XmlWriterSettings.hpp"

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

// ===========================================================================
// #2084 — the DOCTYPE ExternalID quoted literals
//
// #2076 repaired every NAME door and deliberately left the LITERAL doors open, because the
// repair looked like a delimiter/escaping decision with no repository evidence to settle it.
// Measured (build-probe/2084_probe2_before.log, 17 cases), it is settled by this module's
// own DOCTYPE reader:
//
//   * ParseDoctype's readQuoted accepts BOTH '"' and '\'' and scans to the matching quote,
//     so RE-DELIMITING is read-back-preserving -- not a guess.
//   * It never un-escapes anything, so writing "&quot;" inside a literal would store six
//     literal characters. Escaping is therefore a corruption, not a repair.
//
// Three premise corrections the before-state forced, all recorded in the plan:
//
//   1. The finding names TWO reproductions; SEVEN of the 17 cases were broken, and FIVE of
//      the seven failed SILENTLY -- the document parsed and the recovered identifier was
//      simply wrong -- rather than producing unparseable text.
//   2. The finding names one door; XmlDocument::CreateDocumentType builds the same ExternalID
//      by the same raw concatenation and carried the same defect.
//   3. The finding attributes the internal-subset break to a ']'. Measured, the terminator is
//      '>': a bare ']' subset round-trips fine, while the most ordinary subset there is --
//      <!ENTITY a "b"> -- was ALREADY lossy before this ticket. That half is NOT repaired
//      here; see the WriteDocType_InternalSubset_* pins below.
// ===========================================================================

namespace {

/// Round-trips a DOCTYPE through this module's own reader and returns the recovered
/// identifiers, which is a strictly stronger oracle than "LoadXml did not throw": three of
/// the before-state failures parsed cleanly and returned a truncated identifier.
void ExpectDocTypeRoundTrip(const std::string& publicId, const std::string& systemId) {
    auto w = NewWriter();
    ASSERT_NO_THROW(w->WriteDocType("r", publicId, systemId, ""));
    w->WriteStartElement("r");
    w->WriteEndElement();
    const std::string xml = w->ToString();
    ExpectReReadable(xml);

    XmlDocument doc;
    ASSERT_NO_THROW(doc.LoadXml(xml)) << xml;
    auto* dt = doc.getDocumentTypeProperty();
    ASSERT_NE(dt, nullptr) << xml;
    EXPECT_EQ(dt->getPublicIdProperty(), publicId) << xml;
    EXPECT_EQ(dt->getSystemIdProperty(), systemId) << xml;
}

} // namespace

TEST(XmlWriterValidationTests, WriteDocType_PublicIdWithQuote_Throws) {
    // The shipped XmlConvert::VerifyPublicId already rejects '"' -- it is not a PubidChar.
    // Before: emitted <!DOCTYPE r PUBLIC "pu"b" "s"> and the reader recovered "pu".
    auto w = NewWriter();
    try {
        w->WriteDocType("r", "pu\"b", "s", "");
        FAIL() << "expected XmlException";
    } catch (const XmlException& e) {
        EXPECT_NE(e.getMessageProperty().find("public identifier"), std::string::npos)
            << e.getMessageProperty();
    }
}

TEST(XmlWriterValidationTests, WriteDocType_PublicIdApostropheAndControls) {
    // An apostrophe IS a PubidChar and must stay accepted; a control character is not.
    ExpectDocTypeRoundTrip("pub'lic", "sys");
    auto w = NewWriter();
    EXPECT_THROW(w->WriteDocType("r", "a\x01" "b", "", ""), XmlException);
}

TEST(XmlWriterValidationTests, WriteDocType_SystemIdWithQuote_ReDelimitsAndRoundTrips) {
    // The finding's headline case. Before: <!DOCTYPE r SYSTEM "a"b"> recovered "a".
    // After: the literal is re-delimited and the FULL value survives the round trip.
    auto w = NewWriter();
    ASSERT_NO_THROW(w->WriteDocType("r", "", "a\"b", ""));
    w->WriteStartElement("r");
    w->WriteEndElement();
    EXPECT_EQ(w->ToString(), "<!DOCTYPE r SYSTEM 'a\"b'><r/>");
    ExpectDocTypeRoundTrip("", "a\"b");
}

TEST(XmlWriterValidationTests, WriteDocType_SystemIdWithBothQuotes_Throws) {
    // Unrepresentable: XML gives a SystemLiteral exactly two delimiters and no escape.
    auto w = NewWriter();
    try {
        w->WriteDocType("r", "", "a\"b'c", "");
        FAIL() << "expected XmlException";
    } catch (const XmlException& e) {
        EXPECT_NE(e.getMessageProperty().find("cannot be represented"), std::string::npos)
            << e.getMessageProperty();
    }
}

TEST(XmlWriterValidationTests, WriteDocType_SystemIdWithGreaterThan_Throws) {
    // '>' ends the declaration whatever quotes it, because this runtime stores a DOCTYPE as a
    // '>'-terminated node. Both of the finding's own unparseable reproductions reduce to this.
    for (const char* systemId : {"a>b", "s\">x<!--"}) {
        auto w = NewWriter();
        try {
            w->WriteDocType("r", "", systemId, "");
            FAIL() << "expected XmlException for: " << systemId;
        } catch (const XmlException& e) {
            EXPECT_NE(e.getMessageProperty().find("terminate the DOCTYPE"), std::string::npos)
                << e.getMessageProperty();
        }
    }
}

TEST(XmlWriterValidationTests, WriteDocType_SystemIdWithNulOrControl_Throws) {
    // The NUL case is #2085's c_str() truncation reaching a FOURTH door: before this ticket
    // the systemId "a\0b" emitted <!DOCTYPE r SYSTEM "a> -- the closing quote was lost
    // entirely, so the emitted text was malformed, not merely wrong.
    auto w1 = NewWriter();
    EXPECT_THROW(w1->WriteDocType("r", "", std::string("a\0b", 3), ""), XmlException);
    auto w2 = NewWriter();
    EXPECT_THROW(w2->WriteDocType("r", "", "a\x01" "b", ""), XmlException);
    auto w3 = NewWriter();
    EXPECT_THROW(w3->WriteDocType("r", "", "a\x0c" "b", ""), XmlException);
}

TEST(XmlWriterValidationTests, WriteDocType_ValidIdentifiers_ByteIdenticalOutput) {
    // Every value that does not contain a '"' keeps its existing output character for
    // character: '"' stays the preferred delimiter precisely so this holds.
    struct Case { const char* pub; const char* sys; const char* expected; };
    for (const Case& c : {
             Case{"", "", "<!DOCTYPE r><r/>"},
             Case{"", "about:legacy-compat", "<!DOCTYPE r SYSTEM \"about:legacy-compat\"><r/>"},
             Case{"-//W3C//DTD XHTML 1.0//EN", "http://www.w3.org/x.dtd",
                  "<!DOCTYPE r PUBLIC \"-//W3C//DTD XHTML 1.0//EN\" \"http://www.w3.org/x.dtd\"><r/>"},
             Case{"", "sys'tem", "<!DOCTYPE r SYSTEM \"sys'tem\"><r/>"},
             Case{"pub'lic", "", "<!DOCTYPE r PUBLIC \"pub'lic\" \"\"><r/>"},
         }) {
        auto w = NewWriter();
        ASSERT_NO_THROW(w->WriteDocType("r", c.pub, c.sys, ""));
        w->WriteStartElement("r");
        w->WriteEndElement();
        EXPECT_EQ(w->ToString(), c.expected) << c.pub << " | " << c.sys;
    }
}

TEST(XmlWriterValidationTests, WriteDocType_ApostropheInSystemIdKeepsDoubleQuote) {
    // Delimiter PREFERENCE, not merely availability: an apostrophe alone must not flip the
    // literal to single quotes, or every existing caller's output would change.
    auto w = NewWriter();
    ASSERT_NO_THROW(w->WriteDocType("r", "", "sys'tem", ""));
    w->WriteStartElement("r");
    w->WriteEndElement();
    EXPECT_NE(w->ToString().find("SYSTEM \"sys'tem\""), std::string::npos) << w->ToString();
}

TEST(XmlWriterValidationTests, WriteDocType_InternalSubset_NotRepairedHere_SubstrateBounded) {
    // Pins the SCOPE boundary and the corrected premise, so a later reader does not assume
    // the subset half was covered. An ordinary subset containing '>' is accepted and emitted
    // exactly as before -- its output is well-formed XML; only THIS runtime's '>'-terminated
    // DOCTYPE representation loses it on read-back, which XmlDocumentType's doc-comment
    // already concedes. A bare ']', which the finding blamed, is not the terminator at all.
    auto w = NewWriter();
    ASSERT_NO_THROW(w->WriteDocType("r", "", "", "<!ENTITY a \"b\">"));
    w->WriteStartElement("r");
    w->WriteEndElement();
    EXPECT_EQ(w->ToString(), "<!DOCTYPE r [<!ENTITY a \"b\">]><r/>");

    auto w2 = NewWriter();
    ASSERT_NO_THROW(w2->WriteDocType("r", "", "", "]"));
    w2->WriteStartElement("r");
    w2->WriteEndElement();
    EXPECT_EQ(w2->ToString(), "<!DOCTYPE r []]><r/>");
}

// --- the second producer: the DOM door ------------------------------------------------

TEST(XmlWriterValidationTests, CreateDocumentType_SameLiteralRulesAsTheWriterDoor) {
    // Premise correction 2: the finding named only XmlWriter::WriteDocType, but this door
    // built the same ExternalID by the same concatenation. The two must now agree exactly.
    for (const char* systemId : {"a\"b'c", "a>b", "a\x01" "b"}) {
        bool writerThrew = false;
        bool domThrew = false;
        try {
            auto w = NewWriter();
            w->WriteDocType("r", "", systemId, "");
        } catch (const XmlException&) { writerThrew = true; }
        try {
            XmlDocument doc;
            (void)doc.CreateDocumentType("r", "", systemId, "");
        } catch (const XmlException&) { domThrew = true; }
        EXPECT_TRUE(writerThrew) << "systemId: " << systemId;
        EXPECT_EQ(writerThrew, domThrew) << "systemId: " << systemId;
    }
}

TEST(XmlWriterValidationTests, CreateDocumentType_ReDelimitsAndLeavesValidInputUnchanged) {
    XmlDocument doc;
    auto* dt = doc.CreateDocumentType("r", "", "a\"b", "");
    ASSERT_NE(dt, nullptr);
    // The wrapper caches the caller's value; the repair is in the EMITTED text.
    EXPECT_EQ(dt->getSystemIdProperty(), "a\"b");

    XmlDocument doc2;
    EXPECT_THROW((void)doc2.CreateDocumentType("r", "pu\"b", "s", ""), XmlException);
}

// ===========================================================================
// #2085 — an embedded NUL silently truncated writer content
//
// Every writer body hands std::string::c_str() to tinyxml2, whose API is const char*, so the
// byte count died at that boundary. Measured (build-probe/2085_probe1_before.log): the finding
// names THREE doors; SIX lost data, and the caller got no diagnostic from any of them.
//
// Why rejection, and why unconditionally: the XML Char production excludes U+0000, and a
// character reference must itself match Char, so there is NO spelling that carries a NUL
// through a document. "Write it in full" is not an implementable branch, and this runtime's
// own parser already rejects an embedded NUL (XML_ERROR_PARSING_TEXT), so rejecting here makes
// the writer and the reader agree rather than narrowing past them.
//
// Deliberately NOT covered here: the other characters outside Char (0x01, 0x0C, ...). They are
// emitted faithfully rather than lost, and measured (2085_probe2_reader.log) this module's own
// reader ACCEPTS them in text, attributes, CDATA and comments. Both "reject" and "emit" are
// implementable there, which is exactly what makes it an XmlWriterSettings::CheckCharacters
// decision -- a flag can only govern a choice whose branches both exist. That is #2349.
// ===========================================================================

TEST(XmlWriterValidationTests, EveryContentDoor_EmbeddedNul_Throws) {
    // All six measured doors, including the two the finding did not name (WriteComment and
    // WriteProcessingInstruction) and WriteElementString, which inherits WriteString's guard.
    const std::string embedded("a\0b", 3);
    const std::string leading("\0ab", 3);
    for (const std::string& v : {embedded, leading}) {
        { auto w = NewWriter(); w->WriteStartElement("e");
          EXPECT_THROW(w->WriteString(v), XmlException); }
        { auto w = NewWriter(); w->WriteStartElement("e");
          EXPECT_THROW(w->WriteAttributeString("a", v), XmlException); }
        { auto w = NewWriter(); w->WriteStartElement("e");
          EXPECT_THROW(w->WriteCData(v), XmlException); }
        { auto w = NewWriter(); w->WriteStartElement("e");
          EXPECT_THROW(w->WriteComment(v), XmlException); }
        { auto w = NewWriter(); w->WriteStartElement("e");
          EXPECT_THROW(w->WriteProcessingInstruction("p", v), XmlException); }
        { auto w = NewWriter();
          EXPECT_THROW(w->WriteElementString("e", v), XmlException); }
        { auto w = NewWriter(); w->WriteStartElement("e");
          EXPECT_THROW(w->WriteDocType("r", "", "", v), XmlException); }
    }
}

TEST(XmlWriterValidationTests, NulDiagnostic_NamesTheDoorAndTheCause) {
    // The whole point of the repair is that the caller now LEARNS about the loss.
    auto w = NewWriter();
    w->WriteStartElement("e");
    try {
        w->WriteCData(std::string("c\0d", 3));
        FAIL() << "expected XmlException";
    } catch (const XmlException& e) {
        EXPECT_NE(e.getMessageProperty().find("WriteCData"), std::string::npos)
            << e.getMessageProperty();
        EXPECT_NE(e.getMessageProperty().find("NUL"), std::string::npos)
            << e.getMessageProperty();
    }
}

TEST(XmlWriterValidationTests, ContentWithoutNul_ByteIdenticalAtEveryDoor) {
    // The compatibility half: everything that is not a NUL keeps its exact previous output,
    // including valid multi-byte UTF-8 and the three whitespace characters Char allows.
    struct Case { const char* value; const char* expected; };
    for (const Case& c : {
             Case{"ab", "<e>ab</e>"},
             Case{"a\tb\nc", "<e>a\tb\nc</e>"},
             Case{"caf\xc3\xa9", "<e>caf\xc3\xa9</e>"},
             Case{"\xe4\xb8\xad\xe6\x96\x87", "<e>\xe4\xb8\xad\xe6\x96\x87</e>"},
         }) {
        auto w = NewWriter();
        w->WriteStartElement("e");
        ASSERT_NO_THROW(w->WriteString(c.value)) << c.value;
        w->WriteEndElement();
        EXPECT_EQ(w->ToString(), c.expected);
    }
}

TEST(XmlWriterValidationTests, NonNulControlCharacters_StillEmitted_PinnedScopeBoundary) {
    // Pins the SCOPE line so a later reader does not assume #2085 covered the Char production.
    // These characters are outside XML 1.0 Char, so this output is NOT valid XML -- but it is
    // emitted rather than lost, and this module's own reader accepts it. Whether the writer
    // should reject it is #2349's CheckCharacters decision, and this pin fails loudly if that
    // decision is ever made silently.
    auto w = NewWriter();
    w->WriteStartElement("e");
    ASSERT_NO_THROW(w->WriteString("a\x01" "b"));
    w->WriteEndElement();
    EXPECT_EQ(w->ToString(), "<e>a\x01" "b</e>");

    // ... and the flag that would govern it is still documented as not enforced.
    System::Xml::XmlWriterSettings settings;
    EXPECT_TRUE(settings.CheckCharacters);
}
