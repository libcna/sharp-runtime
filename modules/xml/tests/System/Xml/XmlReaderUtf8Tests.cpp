// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <fstream>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>
#include "TestTemporaryDirectory.hpp"
#include "System/ArgumentException.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlReader.hpp"
#include "System/Xml/XmlReaderSettings.hpp"
#include "System/Xml/XmlTextReader.hpp"
#include "System/Xml/XmlWriter.hpp"

using namespace System::Xml;

namespace {
struct BadEncoding { const char* name; std::string bytes; };

// Oracle: UTF-8 shortest forms, continuation 80..BF, scalar <=10FFFF excluding D800..DFFF.
// The UTF-8 bytes are literals, never output captured from SharpRuntime.
const std::vector<BadEncoding> malformed{
    {"lone-continuation-low", "\x80"}, {"lone-continuation-high", "\xBF"},
    {"seed-9949", "\xD9"}, {"truncated-two", "\xC2"},
    {"truncated-three-lead", "\xE2"}, {"truncated-three", "\xE2\x82"},
    {"truncated-four-lead", "\xF0"}, {"truncated-four-two", "\xF0\x90"},
    {"truncated-four-three", "\xF0\x90\x80"},
    {"bad-second-two", "\xC2" "A"}, {"bad-second-three", "\xE2\x28\xA1"},
    {"bad-third-four", "\xF0\x90" "A\x80"},
    {"overlong-nul", "\xC0\x80"}, {"overlong-two", "\xC1\xBF"},
    {"overlong-three", "\xE0\x80\xAF"}, {"overlong-four", "\xF0\x80\x80\xAF"},
    {"surrogate-first", "\xED\xA0\x80"}, {"surrogate-last", "\xED\xBF\xBF"},
    {"above-maximum", "\xF4\x90\x80\x80"}, {"invalid-f5", "\xF5\x80\x80\x80"},
    {"obsolete-five", "\xF8\x88\x80\x80\x80"},
    {"obsolete-six", "\xFC\x84\x80\x80\x80\x80"},
    {"invalid-fe", "\xFE"}, {"invalid-ff", "\xFF"}
};

std::unique_ptr<XmlReader> Open(const std::string& input, int route,
                              const std::string& path, bool checkCharacters = true) {
    XmlReaderSettings settings;
    settings.CheckCharacters = checkCharacters;
    settings.IgnoreComments = true;
    settings.IgnoreProcessingInstructions = true;
    if (route == 0) return std::unique_ptr<XmlReader>(XmlReader::CreateFromString(input));
    if (route == 1) return std::unique_ptr<XmlReader>(XmlReader::Create(input));
    if (route == 2) return std::unique_ptr<XmlReader>(XmlReader::Create(input, settings));
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(input.data(), static_cast<std::streamsize>(input.size()));
        if (!file.good()) throw std::runtime_error("could not write UTF-8 reader fixture");
    }
    if (route == 3) return std::unique_ptr<XmlReader>(XmlReader::Create(path));
    return std::unique_ptr<XmlReader>(XmlReader::Create(path, settings));
}

void RefusesOnRead(XmlReader& reader) {
    EXPECT_EQ(reader.getReadStateProperty(), ReadState::Initial);
    EXPECT_EQ(reader.getNodeTypeProperty(), XmlNodeType::None);
    EXPECT_TRUE(reader.getValueProperty().empty());
    try {
        (void)reader.Read();
        ADD_FAILURE() << "malformed UTF-8 must fail before exposing a node";
    } catch (const XmlException& error) {
        // .NET Xml_InvalidCharInThisEncoding resource; no line/column parity claim here.
        EXPECT_EQ(error.getMessageProperty(), "Invalid character in the given encoding.");
    }
    EXPECT_EQ(reader.getReadStateProperty(), ReadState::Error);
    EXPECT_EQ(reader.getNodeTypeProperty(), XmlNodeType::None);
    EXPECT_TRUE(reader.getNameProperty().empty());
    EXPECT_TRUE(reader.getValueProperty().empty());
    EXPECT_EQ(reader.getAttributeCountProperty(), 0);
    EXPECT_FALSE(reader.Read());
    EXPECT_EQ(reader.getReadStateProperty(), ReadState::Error);
}
}

TEST(XmlReaderUtf8Tests, MalformedTextAndAttributesFailOnReadOnEveryRoute) {
    SharpRuntime::Tests::TestTemporaryDirectory temporary;
    for (const auto& bad : malformed) {
        SCOPED_TRACE(bad.name);
        for (bool attribute : {false, true}) {
            SCOPED_TRACE(attribute);
            const std::string input = attribute ? "<r a=\"" + bad.bytes + "\"/>"
                                                : "<r>" + bad.bytes + "</r>";
            for (int route = 0; route < 5; ++route) {
                SCOPED_TRACE(route);
                std::unique_ptr<XmlReader> reader;
                ASSERT_NO_THROW(reader = Open(input, route, temporary.path("invalid.xml")));
                ASSERT_NE(reader, nullptr);
                RefusesOnRead(*reader);
            }
        }
    }
}

TEST(XmlReaderUtf8Tests, TruncationAtPhysicalEndOfInputIsAnEncodingFailure) {
    SharpRuntime::Tests::TestTemporaryDirectory temporary;
    for (const char* tail : {"\xD9", "\xE2\x82", "\xF0\x90\x80"}) {
        for (int route = 0; route < 5; ++route) {
            SCOPED_TRACE(route);
            std::unique_ptr<XmlReader> reader;
            ASSERT_NO_THROW(reader = Open(std::string("<r>") + tail, route,
                                          temporary.path("truncated.xml")));
            RefusesOnRead(*reader);
        }
    }
}

TEST(XmlReaderUtf8Tests, IgnoredNodesAndBytesAfterNulCannotHideInvalidEncoding) {
    SharpRuntime::Tests::TestTemporaryDirectory temporary;
    const std::vector<std::string> inputs{
        std::string("<!--") + "\xD9" + "--><r/>",
        std::string("<?p ") + "\xD9" + "?><r/>",
        std::string("<r><![CDATA[") + "\xD9" + "]]></r>",
        std::string("<r/>\0", 5) + "\xD9"
    };
    for (const auto& input : inputs) {
        for (int route = 0; route < 5; ++route) {
            SCOPED_TRACE(route);
            auto reader = Open(input, route, temporary.path("hidden.xml"));
            RefusesOnRead(*reader);
        }
    }
}

TEST(XmlReaderUtf8Tests, CheckCharactersFalseStillRefusesMalformedEncoding) {
    // Encoding validity is independent of XML character checking.
    SharpRuntime::Tests::TestTemporaryDirectory temporary;
    for (int route : {2, 4}) {
        auto reader = Open(std::string("<r>") + "\xD9" + "</r>", route,
                           temporary.path("settings.xml"), false);
        RefusesOnRead(*reader);
    }
}

TEST(XmlReaderUtf8Tests, CloseBeforeReadingSuppressesTheDeferredError) {
    SharpRuntime::Tests::TestTemporaryDirectory temporary;
    for (int route = 0; route < 5; ++route) {
        auto reader = Open(std::string("<r>") + "\xD9" + "</r>", route,
                           temporary.path("closed.xml"));
        reader->Close();
        EXPECT_FALSE(reader->Read());
        EXPECT_EQ(reader->getReadStateProperty(), ReadState::Closed);
        EXPECT_EQ(reader->getNodeTypeProperty(), XmlNodeType::None);
    }
}

TEST(XmlReaderUtf8Tests, ScalarBoundariesAndFileBomPreserveExactTextAndAttributes) {
    SharpRuntime::Tests::TestTemporaryDirectory temporary;
    // U+007F,0080,07FF,0800,D7FF,E000,FFFD,10000,10FFFF; embedded U+FEFF is content.
    const std::string value = "\x7F\xC2\x80\xDF\xBF\xE0\xA0\x80\xED\x9F\xBF"
                              "\xEE\x80\x80\xEF\xBF\xBD\xF0\x90\x80\x80"
                              "\xF4\x8F\xBF\xBF\xEF\xBB\xBF";
    const std::string input = "<r a=\"" + value + "\">" + value + "</r>";
    for (int route = 0; route < 5; ++route) {
        SCOPED_TRACE(route);
        for (bool fileBom : {false, true}) {
            if (fileBom && route < 3) continue; // character BOM behavior has its own existing tests
            const auto bytes = (fileBom ? std::string("\xEF\xBB\xBF") : std::string()) + input;
            auto reader = Open(bytes, route, temporary.path("valid.xml"));
            EXPECT_EQ(reader->MoveToContent(), XmlNodeType::Element);
            EXPECT_EQ(reader->getNameProperty(), "r");
            EXPECT_EQ(reader->GetAttribute("a"), value);
            EXPECT_EQ(reader->ReadElementContentAsString(), value);
            EXPECT_FALSE(reader->Read());
            EXPECT_EQ(reader->getReadStateProperty(), ReadState::EndOfFile);
        }
    }
    // Writer consistency: the independently specified valid bytes are preserved verbatim.
    std::unique_ptr<XmlWriter> writer(XmlWriter::CreateToString());
    writer->WriteStartElement("r");
    writer->WriteAttributeString("a", value);
    writer->WriteString(value);
    writer->WriteEndElement();
    EXPECT_EQ(writer->ToString(), input);
}

TEST(XmlReaderUtf8Tests, XmlTextReaderForwardsStringAndFileEncodingFailures) {
    SharpRuntime::Tests::TestTemporaryDirectory temporary;
    const std::string input = std::string("<r>") + "\xD9" + "</r>";
    const auto path = temporary.path("forwarding.xml");
    { std::ofstream file(path, std::ios::binary); file << input; ASSERT_TRUE(file.good()); }
    for (const auto& source : {input, path}) {
        XmlTextReader reader(source);
        EXPECT_THROW((void)reader.Read(), XmlException);
        EXPECT_EQ(reader.getReadStateProperty(), ReadState::Error);
        EXPECT_EQ(reader.getNodeTypeProperty(), XmlNodeType::None);
    }
}

TEST(XmlReaderUtf8Tests, FileReadChunksPreserveSplitScalarsAndRejectFinalTruncation) {
    SharpRuntime::Tests::TestTemporaryDirectory temporary;
    // 3 bytes for <r> + 4092 ASCII bytes = 4095; E2 starts at the last byte of a 4096-byte read.
    const std::string expected = std::string(4092, 'a') + "\xE2\x82\xAC";
    for (int route : {3, 4}) {
        auto reader = Open("<r>" + expected + "</r>", route, temporary.path("split.xml"));
        EXPECT_EQ(reader->MoveToContent(), XmlNodeType::Element);
        EXPECT_EQ(reader->ReadElementContentAsString(), expected);
        reader = Open("<r>" + expected + "\xD9", route, temporary.path("tail.xml"));
        RefusesOnRead(*reader);
    }
}

TEST(XmlReaderUtf8Tests, MissingAndEmptyFilesStillRaiseXmlExceptions) {
    SharpRuntime::Tests::TestTemporaryDirectory temporary;
    for (bool settings : {false, true}) {
        const auto path = temporary.path(settings ? "missing-settings.xml" : "missing.xml");
        EXPECT_THROW({
            std::unique_ptr<XmlReader> reader(settings
                ? XmlReader::Create(path, XmlReaderSettings{}) : XmlReader::Create(path));
        }, XmlException);
    }
    for (int route : {3, 4}) {
        EXPECT_THROW({ auto reader = Open("", route, temporary.path("empty.xml")); }, XmlException);
    }
}
