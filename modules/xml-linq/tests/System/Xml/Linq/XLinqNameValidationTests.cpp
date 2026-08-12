// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #2350 -- the name-grammar half of the two-door asymmetry
// #2201 deliberately left open.
//
// Every element and attribute has two serialization doors. `WriteTo(XmlWriter&)` hands the
// RESOLVED qualified name to System::Xml::XmlWriter, which has routed it through
// XmlConvert::VerifyName since #2076. The direct door -- `SerializeTo(std::ostream&)` behind
// ToString()/ToString(SaveOptions)/Save(fileName), and XAttribute::ToString() -- built the same
// text itself and emitted the name with no grammar check at all. Measured before the repair
// (build-probe/2350_probe1_names.log), 26 of 37 probed names disagreed between the two doors, at
// BOTH the element and the attribute door; the finding named three.
//
// WHAT IS VALIDATED, EXACTLY. Not XName, and not XName::ToString(): the string checked is the
// one ResolveStartTag just produced -- the resolved qualified name (`c`, `p:c`, `xmlns:p`) --
// which is the only name a serializer ever writes. XName::ToString()'s Clark notation
// ("{uri}local") contains '{' and '}' and would fail VerifyName, but no door emits it, so
// validating it would have been a fabricated break. This is the boundary #2196 (the PI target)
// and #2200 (the DOCTYPE name) already hold: validate where the text is produced, never at
// construction. Constructing, storing, mutating, comparing and hashing an invalid XName are all
// still legal -- pinned below -- exactly as they were.
//
// WHY THIS BREAKS NO PARSED TREE. Measured (build-probe/2350_probe3_roundtrip.log), 12 of 12
// parsed documents survive the writer door, so VerifyName accepts every resolved name this
// runtime's parser can produce, including namespace-prefixed and UTF-8 names -- the predicates
// treat every byte >= 128 as a name character. The parser rejects `<1bad/>`, `<.lead/>`,
// `<-lead/>`, `<a$b/>` and `<r 1x='v'/>` outright and normalizes `<:lead/>` to `lead`, so it
// never builds a name this door now refuses. Zero first-party production call sites construct
// one either (0 of 607 scanned name-literal sites).
//
// THE ONE GENUINE NARROWING. A name with a LEADING COLON (":x") used to emit from the direct
// door and could be read back -- but not faithfully: the reader silently renames it, so the
// round trip already lost the colon. It is now rejected at both doors, which is the same extra
// narrowing #2076 recorded for the writer door and Migration-XmlStrictnessAndLifecycle.md §2
// documents. Everything else this door now rejects produced output this runtime's own reader
// already refused to parse.

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "System/ArgumentException.hpp"
#include "System/Xml/Linq/XAttribute.hpp"
#include "System/Xml/Linq/XDocument.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XName.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "System/Xml/XmlWriterSettings.hpp"

using namespace System::Xml::Linq;

namespace {

    /** @brief Serializes @p element through the writer door, so both doors can be compared. */
    bool writerDoorAccepts(const XElement& element) {
        try {
            System::Xml::XmlWriterSettings settings;
            std::unique_ptr<System::Xml::XmlWriter> writer(
                System::Xml::XmlWriter::CreateToString(settings));
            element.WriteTo(*writer);
            writer->Flush();
            return true;
        } catch (const System::Exception&) {
            return false;
        }
    }

    bool directDoorAccepts(const XElement& element) {
        try {
            (void)element.ToString();
            return true;
        } catch (const System::Exception&) {
            return false;
        }
    }

} // namespace

// --- The three names the ticket itself measured ------------------------------------------

TEST(XLinqNameValidationTests, ElementNameWithASpaceIsRejectedByTheDirectDoor) {
    XElement element{XName("a b")};
    EXPECT_THROW((void)element.ToString(), System::Xml::XmlException);
}

TEST(XLinqNameValidationTests, ElementNameStartingWithADigitIsRejectedByTheDirectDoor) {
    XElement element{XName("1bad")};
    EXPECT_THROW((void)element.ToString(), System::Xml::XmlException);
}

TEST(XLinqNameValidationTests, ElementNameContainingAngleBracketIsRejectedByTheDirectDoor) {
    XElement element{XName("a<b")};
    EXPECT_THROW((void)element.ToString(), System::Xml::XmlException);
}

// --- Invalid leading characters ----------------------------------------------------------

TEST(XLinqNameValidationTests, InvalidLeadingCharactersAreRejectedByTheDirectDoor) {
    for (const char* name : {"-lead", ".lead", ":lead", "1lead"}) {
        XElement element{XName(name)};
        EXPECT_THROW((void)element.ToString(), System::Xml::XmlException) << name;
    }
}

// --- Attribute names, both direct doors --------------------------------------------------

TEST(XLinqNameValidationTests, InvalidAttributeNameIsRejectedWhenSerializedWithItsElement) {
    XElement element{XName("r")};
    element.Add(std::make_shared<XAttribute>(XName("a b"), std::string("v")));
    EXPECT_THROW((void)element.ToString(), System::Xml::XmlException);
}

TEST(XLinqNameValidationTests, InvalidAttributeNameIsRejectedByXAttributeToString) {
    // XAttribute::ToString is a direct serializer in its own right, reached by no XNode door.
    XAttribute attribute{XName("a b"), std::string("v")};
    EXPECT_THROW((void)attribute.ToString(), System::Xml::XmlException);
}

TEST(XLinqNameValidationTests, InvalidAttributeNameIsRejectedByXAttributeToStringWhenQualified) {
    // The qualified branch emits `p1:local` plus its own declaration; the caller-supplied half
    // is still the part that has to be a name.
    XAttribute attribute{XName("urn:x", "a b"), std::string("v")};
    EXPECT_THROW((void)attribute.ToString(), System::Xml::XmlException);
}

// --- An empty name reports as the writer door reports it ---------------------------------

TEST(XLinqNameValidationTests, DefaultConstructedNameIsRejectedInsteadOfEmittingAnEmptyTag) {
    // XName's default constructor bypasses XName::Get, so it yields an EMPTY name without
    // throwing. Before #2350 the direct door serialized that element as "</>"; the writer door
    // already threw. VerifyName reports an empty name as ArgumentException, so both doors now
    // agree on the type as well as on the rejection.
    XElement element{XName()};
    EXPECT_THROW((void)element.ToString(), System::ArgumentException);
    EXPECT_FALSE(writerDoorAccepts(element));
}

// --- Valid names keep working, unchanged -------------------------------------------------

TEST(XLinqNameValidationTests, ValidAsciiNamesStillSerialize) {
    EXPECT_EQ(XElement{XName("a")}.ToString(), "<a/>");
    EXPECT_EQ(XElement{XName("abc")}.ToString(), "<abc/>");
    EXPECT_EQ(XElement{XName("_x")}.ToString(), "<_x/>");
    EXPECT_EQ(XElement{XName("a-b")}.ToString(), "<a-b/>");
    EXPECT_EQ(XElement{XName("a.b")}.ToString(), "<a.b/>");
    EXPECT_EQ(XElement{XName("A_1.2-3")}.ToString(), "<A_1.2-3/>");
}

TEST(XLinqNameValidationTests, ValidNamesWithAttributesAreByteIdentical) {
    XElement element{XName("r")};
    element.Add(std::make_shared<XAttribute>(XName("x"), std::string("1")));
    element.Add(std::make_shared<XAttribute>(XName("y-2.z"), std::string("2")));
    EXPECT_EQ(element.ToString(), "<r x=\"1\" y-2.z=\"2\"/>");
}

TEST(XLinqNameValidationTests, Utf8NamesAreStillAccepted) {
    // IsStartNCNameChar/IsNCNameChar treat every byte >= 128 as a name character, so this port's
    // permissive UTF-8 handling is unchanged -- the largest theoretical break does not exist.
    EXPECT_EQ(XElement{XName("caf\xc3\xa9")}.ToString(), "<caf\xc3\xa9/>");
    EXPECT_EQ(XElement{XName("\xe6\x97\xa5\xe6\x9c\xac")}.ToString(), "<\xe6\x97\xa5\xe6\x9c\xac/>");
}

TEST(XLinqNameValidationTests, NamespaceQualifiedTreesRoundTripThroughBothDoors) {
    for (const char* xml : {"<root xmlns='urn:d'><child/></root>",
                            "<root xmlns:p='urn:p'><p:child/></root>",
                            "<root xmlns:p='urn:p' p:attr='v'><p:c><p:d q='1'/></p:c></root>",
                            "<a xmlns:x='urn:x'><x:y x:z='1'/></a>"}) {
        auto document = XDocument::Parse(xml);
        ASSERT_TRUE(document && document->getRootProperty()) << xml;
        EXPECT_TRUE(writerDoorAccepts(*document->getRootProperty())) << xml;
        EXPECT_TRUE(directDoorAccepts(*document->getRootProperty())) << xml;
    }
}

TEST(XLinqNameValidationTests, ADeclarationAttributeStillRendersAsItself) {
    // `xmlns` and `xmlns:p` are names this door chooses, and both must survive the check.
    auto document = XDocument::Parse("<root xmlns:p='urn:p'><p:c/></root>");
    ASSERT_TRUE(document && document->getRootProperty());
    const std::string text = document->getRootProperty()->ToString();
    EXPECT_NE(text.find("xmlns:p=\"urn:p\""), std::string::npos) << text;
}

// --- The two doors now agree -------------------------------------------------------------

TEST(XLinqNameValidationTests, BothDoorsAgreeOnEveryProbedName) {
    // The measured before-state was 26 disagreements of 37 at each door; this pins zero.
    const char* names[] = {"a",     "abc",   "_x",    "a-b",  "a.b",   "a1",    "ns:local",
                           "A_1.2-3", "caf\xc3\xa9", "a b", "1bad", "a<b",  "a>b",   "a&b",
                           "a\"b",  "a'b",   "a/b",   "a=b",  "-lead", ".lead", ":lead",
                           "a(b",   "a[b",   "a{b",   "a|b",  "a%b",   "a#b",   "a!b",
                           "a?b",   "a*b",   "a+b",   "a,b",  "a;b"};
    for (const char* name : names) {
        XElement element{XName(name)};
        EXPECT_EQ(writerDoorAccepts(element), directDoorAccepts(element)) << name;
    }
}

// --- The object model is untouched -------------------------------------------------------

TEST(XLinqNameValidationTests, AnInvalidNameIsStillConstructibleStorableAndMutable) {
    // #2350 deliberately validates at serialization, not at construction, so every state that
    // was reachable before is still reachable. Only asking for the TEXT throws.
    XName invalid("a b");
    EXPECT_EQ(invalid.getLocalNameProperty(), "a b");

    XElement element{XName("ok")};
    element.setNameProperty(invalid); // the mutation door is unchanged
    EXPECT_EQ(element.getNameProperty().getLocalNameProperty(), "a b");
    EXPECT_EQ(element.getNameProperty(), invalid);
    EXPECT_EQ(invalid.GetHashCode(), XName("a b").GetHashCode());

    element.setNameProperty(XName("ok")); // and it serializes again once the name is valid
    EXPECT_EQ(element.ToString(), "<ok/>");
}
