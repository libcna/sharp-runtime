// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #2201 -- the Xml.Linq half of #2085, and its MIRROR IMAGE.
//
// Every X* node kind has two serialization doors. `WriteTo(XmlWriter&)` hands the value to
// System::Xml::XmlWriter, which passes it to tinyxml2's `const char*` API -- so until #2085 an
// embedded NUL silently TRUNCATED the value there, and #2085 made that door reject it. The
// direct door, `SerializeTo(std::ostream&)` behind ToString()/ToString(SaveOptions)/
// Save(fileName), has no `const char*` boundary at all, so it did the opposite: it EMITTED the
// NUL. A three-byte text value serialized to three bytes, one of them NUL.
//
// Neither outcome is usable, and measured (build-probe/2201_probe1_doors.log) this runtime's own
// reader rejects every shape of it:
//   <r>a\0b</r>              XML_ERROR_PARSING_TEXT
//   <r a="x\0y"/>            XML_ERROR_PARSING_ATTRIBUTE
//   <r><![CDATA[a\0b]]></r>  XML_ERROR_PARSING_CDATA
//   <r><!--a\0b--></r>       XML_ERROR_PARSING_COMMENT
//
// Rejection is not a policy choice: the XML 1.0 `Char` production excludes U+0000, and a
// character reference must itself match `Char`, so no spelling -- literal or escaped -- carries a
// NUL through a document. "Emit it in full" is not an implementable branch, which is why this
// needs no XmlWriterSettings flag. The single detector, System::Xml::detail::ContainsNul, is
// shared with the writer door; this component adds only the throwing wrapper that names the
// Linq member.
//
// PREMISE CORRECTION. #2201's description names XText and XCData. Measured, NINE direct doors
// emitted a NUL: XText, XCData, XComment, XProcessingInstruction (data), XDocumentType (internal
// subset), XAttribute::ToString (name, namespace name and value), XElement's start tag (element
// name, attribute name and attribute value) and XDeclaration::ToString (version, encoding,
// standalone). All are covered here. Three others already rejected before this ticket and are
// pinned as such: the PI target and the DOCTYPE name/identifiers, through the name and
// identifier validators, and XStreamingElement, which routes through XmlWriter.
//
// SCOPE. This is the NUL clause and nothing else. Characters outside `Char` OTHER than NUL are
// still emitted, exactly as before -- both branches are implementable there and this module's own
// reader accepts them, so that is a XmlWriterSettings::CheckCharacters decision tracked
// separately. NonNulControlCharacters_StillEmittedByTheDirectDoor below pins that boundary, the
// mirror of the pin #2085 left at the writer door. This clause also does not impose the XML name
// grammar on an element or attribute name at the direct door: it rejects a NUL there and nothing
// more. (Ticket #2350 later did impose that grammar, at the same two doors and with the same
// boundary -- see XLinqNameValidationTests. The NUL guard still runs FIRST, so every diagnostic
// pinned below is unchanged.)

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "System/Xml/Linq/SaveOptions.hpp"
#include "System/ArgumentException.hpp"
#include "System/Xml/Linq/XAttribute.hpp"
#include "System/Xml/Linq/XCData.hpp"
#include "System/Xml/Linq/XComment.hpp"
#include "System/Xml/Linq/XDocument.hpp"
#include "System/Xml/Linq/XDocumentType.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XName.hpp"
#include "System/Xml/Linq/XNode.hpp"
#include "System/Xml/Linq/XProcessingInstruction.hpp"
#include "System/Xml/Linq/XStreamingElement.hpp"
#include "System/Xml/Linq/XText.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlWriter.hpp"

using System::Xml::XmlException;
using System::Xml::XmlWriter;
using System::Xml::Linq::SaveOptions;
using System::Xml::Linq::XAttribute;
using System::Xml::Linq::XCData;
using System::Xml::Linq::XComment;
using System::Xml::Linq::XDeclaration;
using System::Xml::Linq::XDocument;
using System::Xml::Linq::XDocumentType;
using System::Xml::Linq::XElement;
using System::Xml::Linq::XName;
using System::Xml::Linq::XNode;
using System::Xml::Linq::XProcessingInstruction;
using System::Xml::Linq::XStreamingElement;
using System::Xml::Linq::XText;

namespace {

    const std::string kLead("\0ab", 3);
    const std::string kMid("a\0b", 3);
    const std::string kTail("ab\0", 3);
    const std::string kOnly("\0", 1);

    std::string direct(const XNode& n) { return n.ToString(SaveOptions::DisableFormatting); }

    std::string throughWriter(const XNode& n) {
        std::unique_ptr<XmlWriter> w(XmlWriter::CreateToString());
        w->WriteStartDocument();
        w->WriteStartElement("holder");
        n.WriteTo(*w);
        w->WriteEndElement();
        return w->ToString();
    }

    /// Every NUL position must be rejected, not only the interior one: a leading NUL truncates to
    /// the empty string at the writer door and a trailing one is invisible in a naive comparison.
    template <class MakeNode>
    void ExpectEveryNulPositionRejected(MakeNode make) {
        for (const std::string& v : {kLead, kMid, kTail, kOnly}) {
            EXPECT_THROW((void)direct(*make(v)), XmlException) << "position case len=" << v.size();
        }
    }

} // namespace

// --- the node kinds the ticket names -----------------------------------------------------------

TEST(XLinqNulRejectionTests, XText_EveryNulPosition_IsRejected) {
    ExpectEveryNulPositionRejected([](const std::string& v) { return std::make_shared<XText>(v); });
}

TEST(XLinqNulRejectionTests, XText_DiagnosticNamesTheDoorAndTheCause) {
    try {
        (void)direct(XText(kMid));
        FAIL() << "expected XmlException";
    } catch (const XmlException& e) {
        EXPECT_NE(e.getMessageProperty().find("XText::SerializeTo"), std::string::npos)
            << e.getMessageProperty();
        EXPECT_NE(e.getMessageProperty().find("NUL character"), std::string::npos)
            << e.getMessageProperty();
    }
}

TEST(XLinqNulRejectionTests, XCData_EveryNulPosition_IsRejected) {
    // XCData derives from XText but OVERRIDES SerializeTo, so the base class's guard never runs
    // for it -- it needs and has its own.
    ExpectEveryNulPositionRejected([](const std::string& v) { return std::make_shared<XCData>(v); });
    try {
        (void)direct(XCData(kMid));
        FAIL() << "expected XmlException";
    } catch (const XmlException& e) {
        EXPECT_NE(e.getMessageProperty().find("XCData::SerializeTo"), std::string::npos)
            << e.getMessageProperty();
    }
}

// --- the doors the ticket's description did not name --------------------------------------------

TEST(XLinqNulRejectionTests, XComment_EveryNulPosition_IsRejected) {
    ExpectEveryNulPositionRejected([](const std::string& v) { return std::make_shared<XComment>(v); });
}

TEST(XLinqNulRejectionTests, XProcessingInstruction_DataIsRejected_TargetAlreadyWas) {
    ExpectEveryNulPositionRejected(
        [](const std::string& v) { return std::make_shared<XProcessingInstruction>("t", v); });
    // The TARGET needed no new guard: XmlConvert::VerifyName has rejected a NUL there since
    // #2196, because it is not an NCName character. Pinned so the guard is not "helpfully" added
    // twice, and so the claim is checked rather than assumed.
    EXPECT_THROW((void)direct(XProcessingInstruction(kMid, "d")), XmlException);
}

TEST(XLinqNulRejectionTests, XDocumentType_InternalSubsetIsRejected_TheOtherThreeFieldsAlreadyWere) {
    for (const std::string& v : {kLead, kMid, kTail, kOnly})
        EXPECT_THROW((void)direct(XDocumentType("r", "", "", v)), XmlException) << v.size();
    try {
        (void)direct(XDocumentType("r", "", "", kMid));
        FAIL() << "expected XmlException";
    } catch (const XmlException& e) {
        EXPECT_NE(e.getMessageProperty().find("internal subset"), std::string::npos)
            << e.getMessageProperty();
    }
    // #2200's three ExternalID validators already reject a NUL in the other fields, exactly as
    // they do at the writer door. Pinned so this ticket's scope split stays legible.
    EXPECT_THROW((void)direct(XDocumentType(kMid, "", "", "")), XmlException);
    EXPECT_THROW((void)direct(XDocumentType("r", kMid, "s", "")), XmlException);
    EXPECT_THROW((void)direct(XDocumentType("r", "", kMid, "")), XmlException);
}

TEST(XLinqNulRejectionTests, XAttribute_OwnToStringDoor_IsRejected) {
    // XAttribute::ToString() is a direct serializer in its own right -- it is not reached through
    // any XNode::SerializeTo -- so it carries its own guard.
    EXPECT_THROW((void)XAttribute(XName("a"), kMid).ToString(), XmlException);
    EXPECT_THROW((void)XAttribute(XName(kMid), "v").ToString(), XmlException);
    try {
        (void)XAttribute(XName("a"), kMid).ToString();
        FAIL() << "expected XmlException";
    } catch (const XmlException& e) {
        EXPECT_NE(e.getMessageProperty().find("XAttribute::ToString"), std::string::npos)
            << e.getMessageProperty();
    }
}

TEST(XLinqNulRejectionTests, XElement_StartTagParts_AreRejectedAndNamedSeparately) {
    // element name
    {
        auto e = std::make_shared<XElement>(XName(kMid));
        try {
            (void)direct(*e);
            FAIL() << "expected XmlException";
        } catch (const XmlException& ex) {
            EXPECT_NE(ex.getMessageProperty().find("element name"), std::string::npos)
                << ex.getMessageProperty();
        }
    }
    // attribute name
    {
        auto e = std::make_shared<XElement>(XName("r"));
        e->Add(std::make_shared<XAttribute>(XName(kMid), "v"));
        try {
            (void)direct(*e);
            FAIL() << "expected XmlException";
        } catch (const XmlException& ex) {
            EXPECT_NE(ex.getMessageProperty().find("attribute name"), std::string::npos)
                << ex.getMessageProperty();
        }
    }
    // attribute value
    {
        auto e = std::make_shared<XElement>(XName("r"));
        e->Add(std::make_shared<XAttribute>(XName("a"), kMid));
        try {
            (void)direct(*e);
            FAIL() << "expected XmlException";
        } catch (const XmlException& ex) {
            EXPECT_NE(ex.getMessageProperty().find("attribute value"), std::string::npos)
                << ex.getMessageProperty();
        }
    }
}

TEST(XLinqNulRejectionTests, XElement_TextValue_IsRejectedThroughItsXTextChild) {
    auto e = std::make_shared<XElement>(XName("r"));
    e->setValueProperty(kMid);
    EXPECT_THROW((void)direct(*e), XmlException);
}

TEST(XLinqNulRejectionTests, XDeclaration_AllThreeFields_AreRejected) {
    EXPECT_THROW((void)XDeclaration(kMid, "utf-8", "").ToString(), XmlException);
    EXPECT_THROW((void)XDeclaration("1.0", kMid, "").ToString(), XmlException);
    EXPECT_THROW((void)XDeclaration("1.0", "utf-8", kMid).ToString(), XmlException);
    // and through the containing document door
    auto doc = std::make_shared<XDocument>(std::make_shared<XDeclaration>("1.0", kMid, ""),
                                           std::make_shared<XElement>(XName("r")));
    EXPECT_THROW((void)doc->ToString(SaveOptions::DisableFormatting), XmlException);
}

TEST(XLinqNulRejectionTests, XStreamingElement_AlreadyRejected_BecauseItRoutesThroughTheWriter) {
    XStreamingElement se(XName("r"));
    se.Add(kMid);
    EXPECT_THROW((void)se.ToString(SaveOptions::DisableFormatting), XmlException);
}

// --- the two doors now give the same answer -------------------------------------------------------

TEST(XLinqNulRejectionTests, BothDoorsRejectTheSameValues) {
    // The mirror image is closed: before, the writer door threw (since #2085) while the direct
    // door emitted. Now neither produces a document.
    for (const std::string& v : {kLead, kMid, kTail}) {
        EXPECT_THROW((void)direct(XText(v)), XmlException);
        EXPECT_THROW((void)throughWriter(XText(v)), XmlException);
        EXPECT_THROW((void)direct(XCData(v)), XmlException);
        EXPECT_THROW((void)throughWriter(XCData(v)), XmlException);
        EXPECT_THROW((void)direct(XComment(v)), XmlException);
        EXPECT_THROW((void)throughWriter(XComment(v)), XmlException);
        EXPECT_THROW((void)direct(XProcessingInstruction("t", v)), XmlException);
        EXPECT_THROW((void)throughWriter(XProcessingInstruction("t", v)), XmlException);
    }
}

// --- the containing doors -------------------------------------------------------------------------

TEST(XLinqNulRejectionTests, TheGuardAppliesThroughTheDocumentAndFileDoors) {
    auto doc = std::make_shared<XDocument>();
    auto root = std::make_shared<XElement>(XName("r"));
    root->Add(std::make_shared<XText>(kMid));
    doc->Add(root);
    EXPECT_THROW((void)doc->ToString(SaveOptions::DisableFormatting), XmlException);
    EXPECT_THROW((void)doc->ToString(), XmlException);

    const std::string path = "build-tmp/2201_nul_save.xml";
    std::remove(path.c_str());
    EXPECT_THROW(doc->Save(path, SaveOptions::DisableFormatting), XmlException);
    // The file may exist and hold the partial prefix written before the offending child was
    // reached -- that is unchanged by this ticket and is how every mid-stream rejection in this
    // component behaves. What must NOT happen is a completed file carrying a NUL.
    std::ifstream in(path, std::ios::binary);
    if (in.good()) {
        std::stringstream buf;
        buf << in.rdbuf();
        EXPECT_EQ(buf.str().find('\0'), std::string::npos) << "a saved file carried a NUL";
    }
    in.close();
    std::remove(path.c_str());
}

TEST(XLinqNulRejectionTests, ARejectedStartTagWritesNothingToTheStream) {
    // The element's own start tag is guarded before its first byte, so a rejected element does
    // not leave a half-open tag behind. (A NUL in a CHILD is necessarily detected after the
    // parent's start tag was already streamed; that is the ordinary mid-stream case above.)
    std::ostringstream os;
    os << "PREFIX";
    auto e = std::make_shared<XElement>(XName("r"));
    e->Add(std::make_shared<XAttribute>(XName("a"), kMid));
    EXPECT_THROW(static_cast<const XNode&>(*e).SerializeTo(os, 0, false), XmlException);
    EXPECT_EQ(os.str(), "PREFIX");
}

// --- controls: nothing else changed ------------------------------------------------------------------

TEST(XLinqNulRejectionTests, ContentWithoutANul_IsByteIdenticalAtEveryDoor) {
    // The whole compatibility claim of this ticket in one place: tab/CR/LF, multi-byte UTF-8 and
    // the three lexical sequences #2196 repairs all keep exactly the bytes they had.
    EXPECT_EQ(direct(XText("plain")), "plain");
    EXPECT_EQ(direct(XText("a\tb\r\nc")), "a\tb\r\nc");
    EXPECT_EQ(direct(XText("\xC3\xA9\xE2\x82\xAC")), "\xC3\xA9\xE2\x82\xAC");
    EXPECT_EQ(direct(XText("a<b>&c")), "a&lt;b&gt;&amp;c");
    EXPECT_EQ(direct(XCData("left]]>right")), "<![CDATA[left]]]]><![CDATA[>right]]>");
    EXPECT_EQ(direct(XComment("left--right")), "<!--left- -right-->");
    EXPECT_EQ(direct(XProcessingInstruction("p", "left?>right")), "<?p left? >right?>");
    EXPECT_EQ(direct(XDocumentType("r", "", "sys", "")), "<!DOCTYPE r SYSTEM \"sys\">");
    EXPECT_EQ(XAttribute(XName("a"), "a\tb").ToString(), "a=\"a&#x9;b\"");
    EXPECT_EQ(XDeclaration("1.0", "utf-8", "yes").ToString(),
              "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>");
    EXPECT_EQ(XDeclaration("", "", "").ToString(), "<?xml?>");

    auto e = std::make_shared<XElement>(XName("r"));
    e->Add(std::make_shared<XAttribute>(XName("a"), "v"));
    e->Add(std::make_shared<XText>("t"));
    EXPECT_EQ(direct(*e), "<r a=\"v\">t</r>");
}

TEST(XLinqNulRejectionTests, EmptyValuesAreStillAccepted) {
    // The guard tests for a NUL, not for emptiness -- an empty string contains no NUL.
    EXPECT_EQ(direct(XText("")), "");
    EXPECT_EQ(direct(XCData("")), "<![CDATA[]]>");
    EXPECT_EQ(direct(XComment("")), "<!---->");
}

// FLIPPED by #2349 (2026-08-18), the mirror of the writer door's pin flipping in the same change.
// #2201 deliberately did not make the CheckCharacters decision; the reference makes it.
//
// AND IT DISSOLVES THE REASON THIS DOOR WAS THOUGHT TO BE DIFFERENT. #2349 priced option B as
// "not a same-shaped change on both sides", because XNode::SerializeTo takes no settings.
// .NET's does not either: XNode.GetXmlWriterSettings builds a DEFAULT XmlWriterSettings and
// touches only Indent and NamespaceHandling (XNode.cs:681-687), inheriting CheckCharacters = true.
// So this door needs no settings channel of its own, and the two agree by construction.
TEST(XLinqNulRejectionTests, Fix2349_NonCharCodePointsAreRejectedAtTheDirectDoorToo) {
    for (const char* v : {"a\x01" "b", "a\x0c" "b", "a\x1f" "b", "a\x0b" "b"}) {
        const std::string value(v);
        EXPECT_THROW((void)direct(XText(value)), System::ArgumentException)
            << "0x" << static_cast<int>(value[1]);
        EXPECT_THROW((void)direct(XComment(value)), System::ArgumentException);
        EXPECT_THROW((void)XAttribute(XName("a"), value).ToString(), System::ArgumentException);
    }

    // Code points, not bytes: U+FFFE, U+FFFF and a lone surrogate are outside Char too, and a
    // byte-wise check accepts all three.
    for (const char* utf8 : {"\xEF\xBF\xBE", "\xEF\xBF\xBF", "\xED\xA0\x80"}) {
        EXPECT_THROW((void)direct(XText(std::string(utf8))), System::ArgumentException) << utf8;
    }

    // Tab, LF and CR are Char and still pass, and so does ordinary multi-byte text.
    EXPECT_NO_THROW((void)direct(XText(std::string("a\tb\nc\rd"))));
    EXPECT_EQ(direct(XText(std::string("h\xC3\xA9llo"))), "h\xC3\xA9llo");
}

TEST(XLinqNulRejectionTests, ConstructionAndMutationStillAcceptANul) {
    // Same boundary #2196 and #2200 pin: validation lives at the serialization doors, not at
    // construction. The object model still holds a value that only fails when it is written.
    EXPECT_NO_THROW((void)XText(kMid));
    XText t("ok");
    EXPECT_NO_THROW(t.setValueProperty(kMid));
    EXPECT_EQ(t.getValueProperty(), kMid);
    EXPECT_EQ(t.getValueProperty().size(), 3u);
    auto e = std::make_shared<XElement>(XName("r"));
    EXPECT_NO_THROW(e->Add(std::make_shared<XAttribute>(XName("a"), kMid)));
    EXPECT_NO_THROW((void)XDocumentType("r", "", "", kMid));
}
