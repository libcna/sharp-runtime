// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <fstream>
#include <memory>
#include <string>
#include <gtest/gtest.h>
#include "TestTemporaryDirectory.hpp"
#include "System/Xml/XmlDocument.hpp"
#include "System/Xml/XmlElement.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlReader.hpp"
#include "System/Xml/XmlReaderSettings.hpp"

using namespace System::Xml;

namespace {
constexpr const char* bom = "\xEF\xBB\xBF";
constexpr const char* rootError = "Data at the root level is invalid. Line 1, position 1.";

std::unique_ptr<XmlReader> OpenCharacters(const std::string& text, int route) {
    if (route == 0) return std::unique_ptr<XmlReader>(XmlReader::CreateFromString(text));
    if (route == 1) return std::unique_ptr<XmlReader>(XmlReader::Create(text));
    return std::unique_ptr<XmlReader>(XmlReader::Create(text, XmlReaderSettings{}));
}
}

TEST(XmlCharacterInputTests, CharacterPreambleFailsOnReadWithErrorState) {
    // Genuine Framework4/.NET8 evidence: cna-demos XmlReaderEnvelopeOracle.cs.
    for (int route = 0; route < 3; ++route) {
        SCOPED_TRACE(route);
        std::unique_ptr<XmlReader> reader;
        ASSERT_NO_THROW(reader = OpenCharacters(std::string(bom) + "<r/>", route));
        ASSERT_NE(reader, nullptr);
        EXPECT_EQ(reader->getReadStateProperty(), ReadState::Initial);
        EXPECT_EQ(reader->getNodeTypeProperty(), XmlNodeType::None);
        try {
            (void)reader->Read();
            ADD_FAILURE() << "decoded U+FEFF must not be consumed as a byte preamble";
        } catch (const XmlException& error) {
            EXPECT_EQ(error.getMessageProperty(), rootError);
            EXPECT_EQ(error.getLineNumberProperty(), 1);
            EXPECT_EQ(error.getLinePositionProperty(), 1);
        }
        EXPECT_EQ(reader->getReadStateProperty(), ReadState::Error);
        EXPECT_FALSE(reader->Read());
        EXPECT_EQ(reader->getReadStateProperty(), ReadState::Error);
        EXPECT_EQ(reader->getNodeTypeProperty(), XmlNodeType::None);
        reader->Close();
        EXPECT_EQ(reader->getReadStateProperty(), ReadState::Closed);
        EXPECT_FALSE(reader->Read());
    }
}

TEST(XmlCharacterInputTests, CloseBeforeFirstReadDoesNotRaiseTheDeferredError) {
    std::unique_ptr<XmlReader> reader;
    ASSERT_NO_THROW(reader.reset(XmlReader::CreateFromString(std::string(bom) + "<r/>")));
    reader->Close();
    EXPECT_FALSE(reader->Read());
    EXPECT_EQ(reader->getReadStateProperty(), ReadState::Closed);
}

TEST(XmlCharacterInputTests, DomRejectsCharacterPreambleInsteadOfStrippingIt) {
    for (const char* suffix : {"<r/>", "<?xml version=\"1.0\"?><r/>"}) {
        SCOPED_TRACE(suffix);
        XmlDocument document;
        try {
            document.LoadXml(std::string(bom) + suffix);
            ADD_FAILURE() << "LoadXml consumes characters, not encoded bytes";
        } catch (const XmlException& error) {
            EXPECT_EQ(error.getMessageProperty(), rootError);
            EXPECT_EQ(error.getLineNumberProperty(), 1);
            EXPECT_EQ(error.getLinePositionProperty(), 1);
        }
    }
}

TEST(XmlCharacterInputTests, EncodedFilePreambleRemainsAccepted) {
    SharpRuntime::Tests::TestTemporaryDirectory temporary;
    const std::string path = temporary.path("preamble.xml");
    {
        std::ofstream file(path, std::ios::binary);
        file << bom << "<?xml version=\"1.0\"?><r a=\"v\">text</r>";
        ASSERT_TRUE(file.good());
    }
    XmlDocument document;
    ASSERT_NO_THROW(document.Load(path));
    ASSERT_NE(document.getDocumentElementProperty(), nullptr);
    EXPECT_EQ(document.getDocumentElementProperty()->GetAttribute("a"), "v");
    for (int route : {1, 2}) {
        auto reader = OpenCharacters(path, route);
        EXPECT_EQ(reader->MoveToContent(), XmlNodeType::Element);
        EXPECT_EQ(reader->getNameProperty(), "r");
        EXPECT_EQ(reader->GetAttribute("a"), "v");
        EXPECT_EQ(reader->ReadElementContentAsString(), "text");
    }
}

TEST(XmlCharacterInputTests, EmbeddedBomCharactersAreNotRemoved) {
    const std::string input = std::string("<r a=\"") + bom + "\">" + bom + "</r>";
    XmlDocument document;
    ASSERT_NO_THROW(document.LoadXml(input));
    ASSERT_NE(document.getDocumentElementProperty(), nullptr);
    EXPECT_EQ(document.getDocumentElementProperty()->GetAttribute("a"), bom);
    auto reader = OpenCharacters(input, 0);
    EXPECT_EQ(reader->MoveToContent(), XmlNodeType::Element);
    EXPECT_EQ(reader->GetAttribute("a"), bom);
    EXPECT_EQ(reader->ReadElementContentAsString(), bom);
}
