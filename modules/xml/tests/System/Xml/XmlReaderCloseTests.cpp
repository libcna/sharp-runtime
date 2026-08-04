// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2078 / SR-AUD-348 — XmlReader::Close() is a terminal state, not a field.
//
// SR-AUD-348 recorded that a closed reader "can consume input". Measured against 1f2bce9
// (build-probe/2078_probe1_reader_close.cpp, log 2078_probe1_before.log) it was worse and
// wider than filed:
//
//   * Read() had no guard AND assigned ReadState::Interactive on its way out, so the closed
//     state did not survive one call — Close(); Read() reported Interactive on node 'x'.
//   * THREE members destroyed the closed state, not one: Read(), ReadStartElement() and
//     ReadElementContentAsString() (the latter two advance through Read()).
//   * MoveToElement(), MoveToNextAttribute() and GetAttribute() all kept answering.
//
// The repair needs no new state and no new policy: this class ALREADY implements one
// terminal read state correctly. A reader driven past its last event reports EndOfFile,
// returns false from every further Read(), and never leaves that state. Closed is now the
// same shape, and every accessor reuses the class's own pre-existing "no current node"
// answer. Note the reader is NOT streaming — Create() flattens the document into an event
// vector up front — so "consuming input" means advancing a cursor, not reading a stream;
// that does not make the lifecycle boundary less public.
// See docs/SystemXmlNamespaceReviewPlan.md §4.4 and §20.5.
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "System/Xml/ReadState.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlNodeType.hpp"
#include "System/Xml/XmlReader.hpp"

using System::Xml::ReadState;
using System::Xml::XmlException;
using System::Xml::XmlNodeType;
using System::Xml::XmlReader;

namespace {

constexpr const char* kDoc = "<r a='1'><x>t</x><y/></r>";

std::unique_ptr<XmlReader> NewReader() {
    return std::unique_ptr<XmlReader>(XmlReader::CreateFromString(kDoc));
}

/// Every accessor's "there is no current node" answer, asserted as one property.
void ExpectOnNoNode(XmlReader& r) {
    EXPECT_EQ(r.getNodeTypeProperty(), XmlNodeType::None);
    EXPECT_EQ(r.getNameProperty(), "");
    EXPECT_EQ(r.getValueProperty(), "");
    EXPECT_FALSE(r.getIsEmptyElementProperty());
    EXPECT_EQ(r.GetAttribute("a"), "");
}

} // namespace

// ===========================================================================
// #2078 — the filed defect
// ===========================================================================

TEST(XmlReaderCloseTests, ReadAfterCloseReturnsFalseAndDoesNotAdvance) {
    auto r = NewReader();
    ASSERT_TRUE(r->Read());
    ASSERT_EQ(r->getNameProperty(), "r");
    r->Close();
    EXPECT_FALSE(r->Read());
    EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed);
    ExpectOnNoNode(*r); // the cursor did not move to 'x'
}

TEST(XmlReaderCloseTests, ReadStateStaysClosedAcrossManyReads) {
    auto r = NewReader();
    r->Read();
    r->Close();
    for (int i = 0; i < 10; ++i) {
        EXPECT_FALSE(r->Read()) << "iteration " << i;
        EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed) << "iteration " << i;
    }
}

TEST(XmlReaderCloseTests, CloseOnANeverReadReaderIsAlsoTerminal) {
    auto r = NewReader();
    ASSERT_EQ(r->getReadStateProperty(), ReadState::Initial);
    r->Close();
    EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed);
    EXPECT_FALSE(r->Read());
    EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed);
    ExpectOnNoNode(*r);
}

TEST(XmlReaderCloseTests, CloseTwiceIsANoOp) {
    auto r = NewReader();
    r->Read();
    r->Close();
    ASSERT_NO_THROW(r->Close());
    EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed);
    EXPECT_FALSE(r->Read());
}

// ===========================================================================
// #2078 — the two members that destroyed the state through Read()
// ===========================================================================

TEST(XmlReaderCloseTests, ReadStartElementAfterCloseThrowsAndDoesNotAdvance) {
    auto r = NewReader();
    r->Read(); // on <r>
    r->Close();
    EXPECT_THROW(r->ReadStartElement(), XmlException);
    EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed);
    ExpectOnNoNode(*r);
}

TEST(XmlReaderCloseTests, ReadEndElementAfterCloseThrowsAndDoesNotAdvance) {
    auto r = NewReader();
    while (r->Read() && r->getNodeTypeProperty() != XmlNodeType::EndElement) {}
    ASSERT_EQ(r->getNodeTypeProperty(), XmlNodeType::EndElement);
    r->Close();
    EXPECT_THROW(r->ReadEndElement(), XmlException);
    EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed);
}

TEST(XmlReaderCloseTests, ReadElementContentAsStringAfterCloseReturnsEmptyAndDoesNotAdvance) {
    auto r = NewReader();
    r->Read(); // <r>
    r->Read(); // <x>
    ASSERT_EQ(r->getNameProperty(), "x");
    r->Close();
    EXPECT_EQ(r->ReadElementContentAsString(), "");
    EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed);
    ExpectOnNoNode(*r);
}

// ===========================================================================
// #2078 — the three members that kept answering from a closed reader
// ===========================================================================

TEST(XmlReaderCloseTests, NavigationMembersAfterCloseReturnFalse) {
    auto r = NewReader();
    r->Read(); // on <r>, which has attribute a='1'
    ASSERT_TRUE(r->MoveToNextAttribute());
    ASSERT_EQ(r->getValueProperty(), "1");
    r->MoveToElement();
    r->Close();
    EXPECT_FALSE(r->MoveToElement());
    EXPECT_FALSE(r->MoveToNextAttribute());
    EXPECT_EQ(r->GetAttribute("a"), "");
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::None);
}

TEST(XmlReaderCloseTests, AllAccessorsReportNoCurrentNodeAfterClose) {
    auto r = NewReader();
    r->Read();
    r->Read(); // <x>, a non-empty element with text
    ASSERT_EQ(r->getNodeTypeProperty(), XmlNodeType::Element);
    r->Close();
    ExpectOnNoNode(*r);
}

// ===========================================================================
// #2078 — the shape the repair copies: EndOfFile must be UNCHANGED
// ===========================================================================

TEST(XmlReaderCloseTests, EndOfFileIsUnchangedAndIsTheShapeClosedNowCopies) {
    // This is the repository-contained evidence the repair is modelled on: the class
    // already had one correct terminal state. It must keep behaving exactly as before,
    // and it must be DISTINGUISHABLE from Closed.
    auto r = NewReader();
    int events = 0;
    while (r->Read()) ++events;
    EXPECT_EQ(events, 6);
    EXPECT_EQ(r->getReadStateProperty(), ReadState::EndOfFile);
    EXPECT_FALSE(r->Read());
    EXPECT_EQ(r->getReadStateProperty(), ReadState::EndOfFile);
    ExpectOnNoNode(*r);

    r->Close();
    EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed);
}

TEST(XmlReaderCloseTests, ReadingIsUnaffectedUntilCloseIsCalled) {
    // The narrowing must not reach a reader that was never closed: the full traversal,
    // every accessor and every navigation member behave exactly as before.
    auto r = NewReader();
    ASSERT_TRUE(r->Read());
    EXPECT_EQ(r->getNodeTypeProperty(), XmlNodeType::Element);
    EXPECT_EQ(r->getNameProperty(), "r");
    EXPECT_EQ(r->GetAttribute("a"), "1");
    EXPECT_TRUE(r->MoveToNextAttribute());
    EXPECT_EQ(r->getNameProperty(), "a");
    EXPECT_TRUE(r->MoveToElement());
    ASSERT_TRUE(r->Read());
    EXPECT_EQ(r->getNameProperty(), "x");
    EXPECT_EQ(r->ReadElementContentAsString(), "t");
    EXPECT_EQ(r->getReadStateProperty(), ReadState::Interactive);
    EXPECT_EQ(r->getNameProperty(), "y");
    EXPECT_TRUE(r->getIsEmptyElementProperty());
}

TEST(XmlReaderCloseTests, ClosedStateSurvivesEveryPublicMemberAsAProperty) {
    // The property that made SR-AUD-348 possible, asserted directly: NO public member may
    // leave ReadState::Closed. Read() used to.
    const auto closed = [] {
        auto r = NewReader();
        r->Read();
        r->Close();
        return r;
    };
    { auto r = closed(); (void)r->Read();                          EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed); }
    { auto r = closed(); (void)r->MoveToElement();                 EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed); }
    { auto r = closed(); (void)r->MoveToNextAttribute();           EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed); }
    { auto r = closed(); (void)r->GetAttribute("a");               EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed); }
    { auto r = closed(); (void)r->ReadElementContentAsString();    EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed); }
    { auto r = closed(); EXPECT_THROW(r->ReadStartElement(), XmlException); EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed); }
    { auto r = closed(); EXPECT_THROW(r->ReadEndElement(), XmlException);   EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed); }
    { auto r = closed(); (void)r->getNodeTypeProperty(); (void)r->getNameProperty();
      (void)r->getValueProperty(); (void)r->getIsEmptyElementProperty();
      EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed); }
    { auto r = closed(); r->Close();                               EXPECT_EQ(r->getReadStateProperty(), ReadState::Closed); }
}
