// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #2200 -- the Xml.Linq half of #2084's DOCTYPE ExternalID
// repair.
//
// XDocumentType has TWO serialization doors and they were never twins:
//
//   * WriteTo(XmlWriter&)      DELEGATES to XmlWriter::WriteDocType, so it inherited #2084's
//                              repair without a single edit in this component;
//   * SerializeTo(ostream&)    -- the door behind ToString(), ToString(SaveOptions) and
//                              Save(fileName) -- DUPLICATED the old three-literal
//                              concatenation and did not.
//
// Measured before the repair (build-probe/2200_probe1_before.log), the two doors disagreed on
// eleven of eighteen probed declarations. The headline reproductions:
//
//   systemId `sys"tem`   direct `<!DOCTYPE root ... "sys"tem">`   writer re-delimited to 'sys"tem'
//   systemId `sys"te'm`  direct emitted it anyway                 writer THREW (unrepresentable)
//   publicId `pub"lic`   direct `<!DOCTYPE root PUBLIC "pub"lic"` writer THREW (not a PubidChar)
//   systemId `sys>tem`   direct `<!DOCTYPE root SYSTEM "sys>tem">` writer THREW (ends the decl)
//   name     `ro ot`     direct `<!DOCTYPE ro ot>`                writer THREW (not an XML name)
//
// The ticket's own example, XDocumentType("root","pub\"lic","sys\"tem","]>"), serialized as
//   <!DOCTYPE root PUBLIC "pub"lic" "sys"tem" []>]>
// which no XML parser can read as one declaration.
//
// Why re-delimiting and not escaping: a DOCTYPE SystemLiteral/PubidLiteral has no escape
// mechanism at all. This runtime's own DOCTYPE reader (ParseDoctype's readQuoted) ends a
// literal at the first occurrence of its opening quote and never un-escapes anything, so
// `&quot;` inside one would store six literal characters rather than a quote. The one
// definition of that decision lives in System/Xml/detail/XmlLexicalSanitizer.hpp and this
// door now calls it -- there is no second copy to drift.
//
// NOT covered here: the DOCTYPE internal subset. Its `>` problem is this runtime's
// `>`-terminated DOCTYPE representation rather than a delimiter choice (an ordinary
// `<!ENTITY a "b">` needs that `>`), it was already lossy before this ticket, and it is
// tracked separately. The pins at the end of this file hold that boundary still.

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "System/ArgumentException.hpp"
#include "System/Xml/Linq/SaveOptions.hpp"
#include "System/Xml/Linq/XDocument.hpp"
#include "System/Xml/Linq/XDocumentType.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XName.hpp"
#include "System/Xml/Linq/XNode.hpp"
#include "System/Xml/XmlDocument.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlWriter.hpp"

using System::Xml::XmlDocument;
using System::Xml::XmlException;
using System::Xml::XmlWriter;
using System::Xml::Linq::SaveOptions;
using System::Xml::Linq::XDocument;
using System::Xml::Linq::XDocumentType;
using System::Xml::Linq::XElement;
using System::Xml::Linq::XName;
using System::Xml::Linq::XNode;

namespace {

    /// The door under repair: ToString() -> SerializeTo(ostream&).
    std::string direct(const XDocumentType& dt) { return dt.ToString(SaveOptions::DisableFormatting); }

    /// The sibling door, which has delegated to XmlWriter::WriteDocType all along.
    std::string throughWriter(const XDocumentType& dt) {
        std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
        w->WriteStartDocument(); // WriteDocType needs an open document to attach to
        dt.WriteTo(*w);
        return w->ToString();
    }

    /// Serializes a whole document through the DIRECT door, re-reads it with this runtime's own
    /// DOM parser and returns the recovered identifiers. Strictly stronger than "it did not
    /// throw": before the repair three of the bad declarations parsed cleanly and handed back a
    /// TRUNCATED identifier, which is the silent half of the defect.
    void ExpectDirectDocTypeRoundTrip(const std::string& publicId, const std::string& systemId) {
        auto doc = std::make_shared<XDocument>();
        doc->Add(std::make_shared<XDocumentType>("r", publicId, systemId, ""));
        doc->Add(std::make_shared<XElement>(XName("r")));
        const std::string xml = doc->ToString(SaveOptions::DisableFormatting);

        XmlDocument dom;
        ASSERT_NO_THROW(dom.LoadXml(xml)) << xml;
        auto* dt = dom.getDocumentTypeProperty();
        ASSERT_NE(dt, nullptr) << xml;
        EXPECT_EQ(dt->getPublicIdProperty(), publicId) << xml;
        EXPECT_EQ(dt->getSystemIdProperty(), systemId) << xml;
    }

} // namespace

// --- the system identifier: re-delimited, not escaped ----------------------------------------

TEST(XLinqDocTypeSerializationTests, SystemIdWithQuote_ReDelimitsAndRoundTrips) {
    // Before: `<!DOCTYPE r SYSTEM "a"b">`, which the reader accepted and truncated to `a`.
    EXPECT_EQ(direct(XDocumentType("r", "", "a\"b", "")), "<!DOCTYPE r SYSTEM 'a\"b'>");
    ExpectDirectDocTypeRoundTrip("", "a\"b");
}

TEST(XLinqDocTypeSerializationTests, SystemIdWithQuote_AlsoReDelimitsInThePublicForm) {
    EXPECT_EQ(direct(XDocumentType("r", "p", "a\"b", "")), "<!DOCTYPE r PUBLIC \"p\" 'a\"b'>");
    ExpectDirectDocTypeRoundTrip("p", "a\"b");
}

TEST(XLinqDocTypeSerializationTests, SystemIdWithBothQuotes_Throws) {
    // Unrepresentable: XML gives a SystemLiteral exactly two delimiters and no escape.
    XDocumentType dt("r", "", "a\"b'c", "");
    try {
        (void)direct(dt);
        FAIL() << "expected XmlException";
    } catch (const XmlException& e) {
        EXPECT_NE(e.getMessageProperty().find("cannot be represented"), std::string::npos)
            << e.getMessageProperty();
    }
}

TEST(XLinqDocTypeSerializationTests, SystemIdWithGreaterThan_Throws) {
    // '>' ends the declaration whatever quotes it, because this runtime stores a DOCTYPE as a
    // '>'-terminated node. A real system identifier is a URI reference and carries %3E instead.
    for (const char* systemId : {"a>b", "s\">x<!--"}) {
        XDocumentType dt("r", "", systemId, "");
        try {
            (void)direct(dt);
            FAIL() << "expected XmlException for: " << systemId;
        } catch (const XmlException& e) {
            EXPECT_NE(e.getMessageProperty().find("terminate the DOCTYPE"), std::string::npos)
                << e.getMessageProperty();
        }
    }
}

TEST(XLinqDocTypeSerializationTests, ApostropheInSystemIdKeepsTheDoubleQuote) {
    // Delimiter PREFERENCE, not merely availability: an apostrophe alone must not flip the
    // literal to single quotes, or every existing caller's bytes would change.
    EXPECT_EQ(direct(XDocumentType("r", "", "sys'tem", "")), "<!DOCTYPE r SYSTEM \"sys'tem\">");
}

// --- the public identifier --------------------------------------------------------------------

TEST(XLinqDocTypeSerializationTests, PublicIdWithQuote_Throws) {
    // '"' is not a PubidChar, so the shipped XmlConvert::VerifyPublicId rejects it outright and
    // the PubidLiteral never needs a delimiter choice. Before: `<!DOCTYPE r PUBLIC "pu"b" "s">`.
    XDocumentType dt("r", "pu\"b", "s", "");
    try {
        (void)direct(dt);
        FAIL() << "expected XmlException";
    } catch (const XmlException& e) {
        EXPECT_NE(e.getMessageProperty().find("public identifier"), std::string::npos)
            << e.getMessageProperty();
    }
}

TEST(XLinqDocTypeSerializationTests, PublicIdWithGreaterThanOrControl_Throws) {
    EXPECT_THROW((void)direct(XDocumentType("r", "pub>lic", "s", "")), XmlException);
    EXPECT_THROW((void)direct(XDocumentType("r", "a\x01" "b", "s", "")), XmlException);
}

TEST(XLinqDocTypeSerializationTests, PublicIdApostrophe_IsAPubidCharAndStaysAccepted) {
    EXPECT_EQ(direct(XDocumentType("r", "pub'lic", "sys", "")),
              "<!DOCTYPE r PUBLIC \"pub'lic\" \"sys\">");
    ExpectDirectDocTypeRoundTrip("pub'lic", "sys");
}

// --- the fourth field the ticket did not name: the DOCTYPE root-element name --------------------

TEST(XLinqDocTypeSerializationTests, InvalidName_IsRejectedByTheDirectDoor) {
    // Premise correction. #2200 names three quoted literals, but the same door emitted
    // `<!DOCTYPE ro ot>` while the sibling WriteTo() door has rejected that name since #2076.
    // Validated here rather than at construction for the reason #2196 gives for the processing
    // instruction target: narrowing construction is a wider accepted-input change.
    for (const char* name : {"ro ot", "1bad", "a<b"}) {
        EXPECT_THROW((void)direct(XDocumentType(name, "", "", "")), XmlException) << name;
    }
    EXPECT_THROW((void)direct(XDocumentType("", "", "", "")), System::ArgumentException);
}

TEST(XLinqDocTypeSerializationTests, AQualifiedNameIsStillAccepted) {
    // VerifyName, not VerifyNCName -- the same grammar XmlWriter::WriteDocType enforces.
    EXPECT_EQ(direct(XDocumentType("ns:r", "", "", "")), "<!DOCTYPE ns:r>");
}

// --- controls: everything already valid keeps its bytes ----------------------------------------

TEST(XLinqDocTypeSerializationTests, ValidDeclarations_AreByteIdenticalToBeforeTheRepair) {
    struct Case { const char* name; const char* pub; const char* sys; const char* expected; };
    for (const Case& c : {
             Case{"r", "", "", "<!DOCTYPE r>"},
             Case{"html", "", "about:legacy-compat", "<!DOCTYPE html SYSTEM \"about:legacy-compat\">"},
             Case{"html", "-//W3C//DTD XHTML 1.0//EN", "http://www.w3.org/x.dtd",
                  "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0//EN\" \"http://www.w3.org/x.dtd\">"},
             Case{"r", "", "sys'tem", "<!DOCTYPE r SYSTEM \"sys'tem\">"},
             Case{"r", "pub'lic", "", "<!DOCTYPE r PUBLIC \"pub'lic\" \"\">"},
         }) {
        EXPECT_EQ(direct(XDocumentType(c.name, c.pub, c.sys, "")), c.expected)
            << c.name << " | " << c.pub << " | " << c.sys;
    }
}

TEST(XLinqDocTypeSerializationTests, IndentedFormIsUnchangedForValidInput) {
    XDocumentType dt("r", "", "sys", "");
    std::ostringstream os;
    static_cast<const XNode&>(dt).SerializeTo(os, 2, /*indent=*/true);
    EXPECT_EQ(os.str(), "    <!DOCTYPE r SYSTEM \"sys\">");
}

// --- the two doors agree ------------------------------------------------------------------------

TEST(XLinqDocTypeSerializationTests, BothDoorsAcceptOrRejectEveryProbedDeclaration) {
    struct Case { const char* name; const char* pub; const char* sys; bool valid; };
    for (const Case& c : {
             Case{"r", "", "", true},
             Case{"r", "-//W3C//DTD//EN", "http://x/y.dtd", true},
             Case{"r", "", "sys'tem", true},
             Case{"r", "pub'lic", "sys", true},
             Case{"r", "", "sys\"tem", true},   // representable: re-delimited by both doors
             Case{"r", "", "sys\"te'm", false}, // unrepresentable
             Case{"r", "pub\"lic", "sys", false},
             Case{"r", "pub>lic", "sys", false},
             Case{"r", "", "sys>tem", false},
             Case{"ro ot", "", "sys", false},
         }) {
        XDocumentType dt(c.name, c.pub, c.sys, "");
        if (c.valid) {
            std::string emitted, written;
            ASSERT_NO_THROW(emitted = direct(dt)) << c.name << '|' << c.pub << '|' << c.sys;
            ASSERT_NO_THROW(written = throughWriter(dt)) << c.name << '|' << c.pub << '|' << c.sys;
            // The writer wraps its output in a document, so the declaration must appear inside
            // it verbatim -- that is what "the two doors emit the same declaration" means here.
            EXPECT_NE(written.find(emitted), std::string::npos)
                << "direct=" << emitted << " writer=" << written;
        } else {
            EXPECT_ANY_THROW((void)direct(dt)) << c.name << '|' << c.pub << '|' << c.sys;
            EXPECT_ANY_THROW((void)throughWriter(dt)) << c.name << '|' << c.pub << '|' << c.sys;
        }
    }
}

// --- the repair reaches every containing door ----------------------------------------------------

TEST(XLinqDocTypeSerializationTests, TheRepairAppliesThroughTheDocumentDoor) {
    auto doc = std::make_shared<XDocument>();
    doc->Add(std::make_shared<XDocumentType>("r", "", "a\"b", ""));
    doc->Add(std::make_shared<XElement>(XName("r")));
    EXPECT_NE(doc->ToString(SaveOptions::DisableFormatting).find("SYSTEM 'a\"b'"), std::string::npos)
        << doc->ToString(SaveOptions::DisableFormatting);

    auto bad = std::make_shared<XDocument>();
    bad->Add(std::make_shared<XDocumentType>("r", "", "a>b", ""));
    bad->Add(std::make_shared<XElement>(XName("r")));
    EXPECT_THROW((void)bad->ToString(SaveOptions::DisableFormatting), XmlException);
}

TEST(XLinqDocTypeSerializationTests, TheRepairAppliesThroughSaveToFile) {
    const std::string path = "build-tmp/2200_doctype_save.xml";
    auto doc = std::make_shared<XDocument>();
    doc->Add(std::make_shared<XDocumentType>("r", "", "a\"b", ""));
    doc->Add(std::make_shared<XElement>(XName("r")));
    ASSERT_NO_THROW(doc->Save(path, SaveOptions::DisableFormatting));

    std::ifstream in(path);
    ASSERT_TRUE(in.good());
    std::stringstream buf;
    buf << in.rdbuf();
    in.close();
    EXPECT_NE(buf.str().find("SYSTEM 'a\"b'"), std::string::npos) << buf.str();
    std::remove(path.c_str());
}

TEST(XLinqDocTypeSerializationTests, ARejectedDeclarationWritesNothingToTheStream) {
    // Validation runs before the first byte, so a caller streaming into an accumulating buffer
    // does not end up with a half-written declaration it cannot see.
    std::ostringstream os;
    os << "PREFIX";
    XDocumentType dt("r", "", "a>b", "");
    EXPECT_THROW(static_cast<const XNode&>(dt).SerializeTo(os, 0, false), XmlException);
    EXPECT_EQ(os.str(), "PREFIX");
}

// --- boundaries this ticket deliberately did not move --------------------------------------------

TEST(XLinqDocTypeSerializationTests, ConstructionAndSettersStillDoNotValidate) {
    // Same boundary #2196 pinned for XProcessingInstruction: validation was added at the
    // serialization doors, not at construction. Pinned so a later ticket cannot move it by
    // accident.
    EXPECT_NO_THROW(XDocumentType("ro ot", "pub\"lic", "a>b", ""));
    XDocumentType dt("r", "", "", "");
    EXPECT_NO_THROW(dt.setSystemIdProperty("a\"b'c"));
    EXPECT_EQ(dt.getSystemIdProperty(), "a\"b'c");
    EXPECT_NO_THROW(dt.setPublicIdProperty("pub\"lic"));
    EXPECT_EQ(dt.getPublicIdProperty(), "pub\"lic");
}

TEST(XLinqDocTypeSerializationTests, InternalSubset_IsEmittedExactlyAsBefore_StillOutsideThisTicket) {
    // Scope pin. An ordinary internal subset contains a '>' that XML REQUIRES, so re-delimiting
    // cannot help it and it is not rejected here either: the bytes emitted are well-formed XML
    // and unchanged by #2200. Only THIS runtime's '>'-terminated DOCTYPE representation loses
    // the subset on read-back, which XmlDocumentType's doc-comment already concedes. A bare ']'
    // is not the terminator at all and round-trips. Tracked separately from #2200.
    EXPECT_EQ(direct(XDocumentType("r", "", "", "<!ENTITY a \"b\">")),
              "<!DOCTYPE r [<!ENTITY a \"b\">]>");
    EXPECT_EQ(direct(XDocumentType("r", "", "", "]")), "<!DOCTYPE r []]>");
    // The same text the sibling writer door emits, character for character.
    EXPECT_NE(throughWriter(XDocumentType("r", "", "", "<!ENTITY a \"b\">"))
                  .find("<!DOCTYPE r [<!ENTITY a \"b\">]>"),
              std::string::npos);
}
