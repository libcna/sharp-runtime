// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #2196 (SR-AUD-335): the direct `SerializeTo` serializers --
// the ones behind ToString(), ToString(SaveOptions) and Save(fileName) -- used to concatenate
// `]]>`, `--` and `?>` without splitting, protecting or rejecting, while the sibling WriteTo()
// door of the *same node objects* already routed through System::Xml::XmlWriter's self-healing
// transforms and through XmlConvert::VerifyName.
//
// Measured before the repair (build-probe/2195_probe1_surface.log,
// build-probe/2195_probe2_malformed.log):
//
//   XCData("left]]>right")                 direct `<![CDATA[left]]>right]]>`   writer already correct
//   XComment("left--right")                direct `<!--left--right-->`        writer already correct
//   XComment("trailing-")                  direct `<!--trailing--->`          writer already correct
//   XProcessingInstruction("p","left?>r")  direct `<?p left?>r?>`             writer already correct
//   XProcessingInstruction("a?>b","d")     direct `<?a?>b d?>`                writer already THREW
//
// Five doors, not the three node kinds the audit index names, and the fifth -- the processing
// instruction TARGET -- failed differently from the other four.
//
// The consequences were also three different shapes, which is why this suite asserts round trips
// and not only emitted text:
//   * CDATA  -- LOSSY. One node re-read as a CDATA node plus a TEXT node; value `left]]>right`
//                came back as `leftright]]>`.
//   * PI     -- UNPARSEABLE. Re-reading threw XML_ERROR_PARSING_DECLARATION.
//   * comment-- SILENT. The invalid comment is accepted by this runtime's parser, so nothing
//                signalled the corruption at all.
//
// Every "the two doors agree" assertion below is the property that keeps them from drifting
// apart again: they now share one definition, in System/Xml/detail/XmlLexicalSanitizer.hpp.

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "TestTemporaryDirectory.hpp"
#include "System/Xml/Linq/SaveOptions.hpp"
#include "System/Xml/Linq/XCData.hpp"
#include "System/Xml/Linq/XComment.hpp"
#include "System/Xml/Linq/XDocument.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XProcessingInstruction.hpp"
#include "System/Xml/Linq/XText.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlWriter.hpp"

using System::Xml::Linq::SaveOptions;
using System::Xml::Linq::XCData;
using System::Xml::Linq::XComment;
using System::Xml::Linq::XDocument;
using System::Xml::Linq::XElement;
using System::Xml::Linq::XName;
using System::Xml::Linq::XNode;
using System::Xml::Linq::XProcessingInstruction;
using System::Xml::Linq::XText;

namespace {

    std::string direct(const XNode& n) { return n.ToString(SaveOptions::DisableFormatting); }

    std::string throughWriter(const XNode& n) {
        std::unique_ptr<System::Xml::XmlWriter> w(System::Xml::XmlWriter::CreateToString());
        n.WriteTo(*w);
        return w->ToString();
    }

    /// Serializes @p child inside an element, re-parses the text, and returns the element's
    /// concatenated text value -- the reading that is stable across a CDATA section split.
    std::string roundTripValue(const std::shared_ptr<XNode>& child) {
        auto holder = std::make_shared<XElement>(XName("r"));
        holder->Add(child);
        auto back = XElement::Parse(holder->ToString(SaveOptions::DisableFormatting));
        return back->getValueProperty();
    }

} // namespace

// --- XCData: the section terminator ---------------------------------------------------------

TEST(XLinqLexicalSerializationTests, CData_EmbeddedTerminator_IsSplitAcrossAdjacentSections) {
    XCData c("left]]>right");
    EXPECT_EQ(direct(c), "<![CDATA[left]]]]><![CDATA[>right]]>");
}

TEST(XLinqLexicalSerializationTests, CData_EmbeddedTerminator_RoundTripsLosslessly) {
    // The measured pre-repair value was "leftright]]>" -- the terminator had moved.
    EXPECT_EQ(roundTripValue(std::make_shared<XCData>("left]]>right")), "left]]>right");
}

TEST(XLinqLexicalSerializationTests, CData_ValueThatIsOnlyTheTerminator_RoundTripsLosslessly) {
    EXPECT_EQ(direct(XCData("]]>")), "<![CDATA[]]]]><![CDATA[>]]>");
    EXPECT_EQ(roundTripValue(std::make_shared<XCData>("]]>")), "]]>");
}

TEST(XLinqLexicalSerializationTests, CData_TwoAdjacentTerminators_RoundTripLosslessly) {
    EXPECT_EQ(roundTripValue(std::make_shared<XCData>("]]>]]>")), "]]>]]>");
}

TEST(XLinqLexicalSerializationTests, CData_TerminatorAtEachPosition_RoundTripsLosslessly) {
    for (const std::string& v : {std::string("]]>tail"), std::string("head]]>"),
                                 std::string("a]]>b]]>c"), std::string("]]")}) {
        EXPECT_EQ(roundTripValue(std::make_shared<XCData>(v)), v) << "value=" << v;
    }
}

TEST(XLinqLexicalSerializationTests, CData_OrdinaryValueIsUntouched) {
    EXPECT_EQ(direct(XCData("plain <text> & more")), "<![CDATA[plain <text> & more]]>");
}

TEST(XLinqLexicalSerializationTests, CData_EmptyValueStillEmitsAnEmptySection) {
    EXPECT_EQ(direct(XCData("")), "<![CDATA[]]>");
}

TEST(XLinqLexicalSerializationTests, CData_BothDoorsEmitTheSameSection) {
    for (const std::string& v : {std::string("left]]>right"), std::string("]]>"),
                                 std::string("]]>]]>"), std::string("plain")}) {
        XCData c(v);
        // The writer wraps its output in a document, so compare the CDATA section itself.
        EXPECT_NE(throughWriter(c).find(direct(c)), std::string::npos)
            << "value=" << v << " direct=" << direct(c) << " writer=" << throughWriter(c);
    }
}

// --- XComment: the double hyphen ------------------------------------------------------------

TEST(XLinqLexicalSerializationTests, Comment_EmbeddedDoubleHyphen_IsSeparatedBySpace) {
    EXPECT_EQ(direct(XComment("left--right")), "<!--left- -right-->");
}

TEST(XLinqLexicalSerializationTests, Comment_TrailingHyphen_DoesNotAbutTheCloseDelimiter) {
    EXPECT_EQ(direct(XComment("trailing-")), "<!--trailing- -->");
}

TEST(XLinqLexicalSerializationTests, Comment_HyphenOnlyValues_AreAllWellFormed) {
    EXPECT_EQ(direct(XComment("-")), "<!--- -->");
    EXPECT_EQ(direct(XComment("--")), "<!--- - -->");
    EXPECT_EQ(direct(XComment("---")), "<!--- - - -->");
}

TEST(XLinqLexicalSerializationTests, Comment_RepairedTextStillContainsNoDoubleHyphen) {
    for (const std::string& v : {std::string("left--right"), std::string("trailing-"),
                                 std::string("--"), std::string("a--b--c-")}) {
        const std::string text = direct(XComment(v));
        const std::string body = text.substr(4, text.size() - 7); // strip "<!--" and "-->"
        EXPECT_EQ(body.find("--"), std::string::npos) << "value=" << v << " emitted=" << text;
        EXPECT_FALSE(!body.empty() && body.back() == '-') << "value=" << v << " emitted=" << text;
    }
}

TEST(XLinqLexicalSerializationTests, Comment_OrdinaryValueIsUntouched) {
    EXPECT_EQ(direct(XComment(" a plain comment ")), "<!-- a plain comment -->");
    EXPECT_EQ(direct(XComment("")), "<!---->");
}

TEST(XLinqLexicalSerializationTests, Comment_BothDoorsEmitTheSameText) {
    for (const std::string& v : {std::string("left--right"), std::string("trailing-"),
                                 std::string("--"), std::string("plain")}) {
        XComment c(v);
        EXPECT_NE(throughWriter(c).find(direct(c)), std::string::npos)
            << "value=" << v << " direct=" << direct(c) << " writer=" << throughWriter(c);
    }
}

// --- XProcessingInstruction: the data ---------------------------------------------------------

TEST(XLinqLexicalSerializationTests, ProcessingInstruction_EmbeddedTerminatorInData_IsSeparated) {
    EXPECT_EQ(direct(XProcessingInstruction("p", "left?>right")), "<?p left? >right?>");
}

TEST(XLinqLexicalSerializationTests, ProcessingInstruction_EmbeddedTerminator_RoundTripsWithoutLosingData) {
    // The measured pre-repair loss (build-probe/2196_probe4_pi.log, case P09): the emitted
    // `<?p left?>right?>` re-read as a processing instruction whose data was just `left` --
    // `right` was silently DROPPED. The repaired text (case P10) re-reads as `left? >right`.
    //
    // The instruction is placed at document level and before everything else because that is
    // the ONLY position this runtime's tinyxml2-backed parser accepts one at all (#2202) --
    // measured separately below so this assertion cannot be misread as covering that.
    auto doc = std::make_shared<XDocument>();
    doc->Add(std::make_shared<XProcessingInstruction>("p", "left?>right"));
    doc->Add(std::make_shared<XElement>(XName("root")));

    std::shared_ptr<XDocument> back;
    ASSERT_NO_THROW(back = XDocument::Parse(doc->ToString(SaveOptions::DisableFormatting)));

    const XProcessingInstruction* pi = nullptr;
    for (const auto& n : back->Nodes())
        if (n->getNodeTypeProperty() == System::Xml::XmlNodeType::ProcessingInstruction)
            pi = static_cast<const XProcessingInstruction*>(n.get());
    ASSERT_NE(pi, nullptr);
    EXPECT_EQ(pi->getTargetProperty(), "p");
    EXPECT_EQ(pi->getDataProperty(), "left? >right");
    EXPECT_NE(pi->getDataProperty(), "left");
}

TEST(XLinqLexicalSerializationTests, ProcessingInstruction_ParserPositionLimitIsSubstrateNotSerialization) {
    // Recorded so the assertion above is not read as claiming more than it does. This
    // runtime's parser accepts a processing instruction only before every other node (an XML
    // declaration may precede it); inside an element, after the root, or even after a comment
    // it throws XML_ERROR_PARSING_DECLARATION. That is #2202, a substrate limitation
    // independent of SR-AUD-335 -- the text emitted here is well-formed XML either way.
    EXPECT_NO_THROW((void)XDocument::Parse("<?p d?><root/>"));
    EXPECT_THROW((void)XDocument::Parse("<root><?p d?></root>"), System::Xml::XmlException);
    EXPECT_THROW((void)XDocument::Parse("<root/><?p d?>"), System::Xml::XmlException);
    EXPECT_THROW((void)XDocument::Parse("<!--c--><?p d?><root/>"), System::Xml::XmlException);
}

TEST(XLinqLexicalSerializationTests, ProcessingInstruction_TrailingQuestionMarkNeedsNoProtection) {
    // The backslash escapes the ?? that would otherwise read as a trigraph introducer.
    EXPECT_EQ(direct(XProcessingInstruction("p", "left?")), "<?p left?\?>");
}

TEST(XLinqLexicalSerializationTests, ProcessingInstruction_EmptyDataOmitsTheSeparator) {
    EXPECT_EQ(direct(XProcessingInstruction("p", "")), "<?p?>");
}

TEST(XLinqLexicalSerializationTests, ProcessingInstruction_OrdinaryDataIsUntouched) {
    EXPECT_EQ(direct(XProcessingInstruction("xml-stylesheet", "href=\"a.xsl\"")),
              "<?xml-stylesheet href=\"a.xsl\"?>");
}

// --- XProcessingInstruction: the target, the fifth door ---------------------------------------

TEST(XLinqLexicalSerializationTests, ProcessingInstruction_MalformedTarget_IsRejectedByTheDirectDoor) {
    // The writer door has always thrown for this; the direct door emitted `<?a?>b d?>`.
    XProcessingInstruction pi("a?>b", "d");
    EXPECT_THROW((void)direct(pi), System::Xml::XmlException);
}

TEST(XLinqLexicalSerializationTests, ProcessingInstruction_BothDoorsRejectTheSameTargets) {
    for (const std::string& target : {std::string("a?>b"), std::string("1bad"), std::string("a b")}) {
        XProcessingInstruction pi(target, "d");
        EXPECT_THROW((void)direct(pi), System::Xml::XmlException) << "target=" << target;
        EXPECT_THROW((void)throughWriter(pi), System::Xml::XmlException) << "target=" << target;
    }
}

TEST(XLinqLexicalSerializationTests, ProcessingInstruction_ConstructionAndSetterStillDoNotValidate) {
    // Deliberately unchanged by #2196 (see the class doc-comment): validation was added at the
    // two serialization doors, not at construction, because narrowing construction is a wider
    // accepted-input change than SR-AUD-335 calls for. This pins that boundary so a later
    // ticket cannot move it by accident.
    EXPECT_NO_THROW(XProcessingInstruction("a?>b", "d"));
    XProcessingInstruction pi("p", "d");
    EXPECT_NO_THROW(pi.setTargetProperty("1bad"));
    EXPECT_EQ(pi.getTargetProperty(), "1bad");
}

TEST(XLinqLexicalSerializationTests, ProcessingInstruction_AQualifiedTargetIsStillAccepted) {
    // VerifyName (not VerifyNCName) is the validator both doors use, so a colon is legal --
    // the same grammar XmlWriter::WriteProcessingInstruction already enforced.
    EXPECT_EQ(direct(XProcessingInstruction("ns:p", "d")), "<?ns:p d?>");
}

// --- Every containing door reaches the same repair --------------------------------------------

TEST(XLinqLexicalSerializationTests, TheRepairAppliesThroughEveryContainingSerializationDoor) {
    auto holder = std::make_shared<XElement>(XName("r"));
    holder->Add(std::make_shared<XCData>("left]]>right"));
    holder->Add(std::make_shared<XComment>("c--d"));

    const std::string compact = holder->ToString(SaveOptions::DisableFormatting);
    const std::string indented = holder->ToString();
    for (const std::string& text : {compact, indented}) {
        EXPECT_NE(text.find("]]]]><![CDATA[>"), std::string::npos) << text;
        EXPECT_NE(text.find("<!--c- -d-->"), std::string::npos) << text;
    }
}

TEST(XLinqLexicalSerializationTests, TheRepairAppliesThroughTheDocumentDoor) {
    auto doc = std::make_shared<XDocument>();
    auto root = std::make_shared<XElement>(XName("r"));
    root->Add(std::make_shared<XCData>("]]>"));
    doc->Add(root);
    EXPECT_NE(doc->ToString(SaveOptions::DisableFormatting).find("]]]]><![CDATA[>"),
              std::string::npos);
}

TEST(XLinqLexicalSerializationTests, TheRepairAppliesThroughSaveToFile) {
    SharpRuntime::Tests::TestTemporaryDirectory temporary;
    const std::string path = temporary.path("save-probe.xml");
    auto root = std::make_shared<XElement>(XName("r"));
    root->Add(std::make_shared<XCData>("left]]>right"));
    root->Add(std::make_shared<XComment>("c--d"));
    ASSERT_NO_THROW(root->Save(path, SaveOptions::DisableFormatting));

    auto reloaded = XElement::Load(path);
    ASSERT_NE(reloaded, nullptr);
    EXPECT_EQ(reloaded->getValueProperty(), "left]]>right");
}

// --- The node kinds this ticket deliberately does NOT repair -----------------------------------

TEST(XLinqLexicalSerializationTests, TextNodeEscapingIsUnchanged) {
    // XText already escapes &, < and >; #2196 did not touch it, and this pins that.
    EXPECT_EQ(direct(XText("a<b>c&d")), "a&lt;b&gt;c&amp;d");
}
