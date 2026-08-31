// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// SAMPLES-DEC-008: the lexical forms a real save file or content file actually carries, and the
// escaping a save file needs when a player names something with an ampersand.
//
// The float cases are not invented. Spacewar's shipped `settings.xml` writes `<X>.4</X>` --
// no leading zero -- inside its ShipLights block, and the original game loads that file, so a
// reader that rejected it would fail on Microsoft's own content.
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>

#include "System/Xml/Serialization/XmlSerializer.hpp"

using System::Xml::Serialization::XmlSerializer;

namespace {

struct Vector4Like {
    float X = 0, Y = 0, Z = 0, W = 0;
    SHARP_XML_SERIALIZABLE(Vector4Like, "Vector4", SHARP_XML_M(Vector4Like, X),
                            SHARP_XML_M(Vector4Like, Y), SHARP_XML_M(Vector4Like, Z),
                            SHARP_XML_M(Vector4Like, W))
    bool operator==(const Vector4Like&) const = default;
};

struct TextHolder {
    std::string Value;
    SHARP_XML_SERIALIZABLE(TextHolder, "TextHolder", SHARP_XML_M(TextHolder, Value))
};

struct NumberHolder {
    std::int32_t Count = 0;
    float Ratio = 0;
    SHARP_XML_SERIALIZABLE(NumberHolder, "NumberHolder", SHARP_XML_M(NumberHolder, Count),
                            SHARP_XML_M(NumberHolder, Ratio))
};

}  // namespace

// The exact ShipLights ambient block from Spacewar's settings.xml, leading dots and all.
TEST(XmlLexicalFormTests, FloatWithoutLeadingZero_ParsesAsSpacewarsSettingsWritesIt) {
    XmlSerializer<Vector4Like> serializer;
    const Vector4Like ambient = serializer.Deserialize(
        "<Ambient><X>.4</X><Y>.4</Y><Z>.4</Z><W>1</W></Ambient>");

    EXPECT_FLOAT_EQ(ambient.X, 0.4f);
    EXPECT_FLOAT_EQ(ambient.Y, 0.4f);
    EXPECT_FLOAT_EQ(ambient.Z, 0.4f);
    EXPECT_FLOAT_EQ(ambient.W, 1.0f);

    // And the values from the second ShipLighting entry, which mixes forms.
    const Vector4Like colour = serializer.Deserialize(
        "<Ambient><X>.639</X><Y>.808</Y><Z>.937</Z><W>1</W></Ambient>");
    EXPECT_FLOAT_EQ(colour.X, 0.639f);
    EXPECT_FLOAT_EQ(colour.Z, 0.937f);
}

TEST(XmlLexicalFormTests, LeadingPlusIsAccepted) {
    XmlSerializer<NumberHolder> serializer;
    const NumberHolder value =
        serializer.Deserialize("<NumberHolder><Count>+42</Count><Ratio>+0.25</Ratio></NumberHolder>");

    // XML Schema's numeric lexical space allows an explicit '+', and .NET accepts it. This
    // reaches std::from_chars underneath, which does not, so the module strips it first. No
    // authentic fixture uses the form; a hand-edited save could.
    EXPECT_EQ(value.Count, 42);
    EXPECT_FLOAT_EQ(value.Ratio, 0.25f);
}

TEST(XmlLexicalFormTests, WhitespaceAroundNumbersIsTolerated) {
    XmlSerializer<NumberHolder> serializer;
    // What an indented, hand-formatted document produces around a value.
    const NumberHolder value = serializer.Deserialize(
        "<NumberHolder>\n  <Count>\n    7\n  </Count>\n  <Ratio> 1.5 </Ratio>\n</NumberHolder>");

    EXPECT_EQ(value.Count, 7);
    EXPECT_FLOAT_EQ(value.Ratio, 1.5f);
}

TEST(XmlLexicalFormTests, InfinityUsesTheSchemaTokensNotDotNetsSpelling) {
    XmlSerializer<NumberHolder> serializer;

    // XML Schema spells infinity INF/-INF; .NET's own Single.ToString spells it "Infinity",
    // which is NOT valid in an xsd:float lexical space. XmlConvert already gets this right and
    // the round trip must preserve it.
    NumberHolder value;
    value.Ratio = std::numeric_limits<float>::infinity();
    const std::string xml = serializer.Serialize(value);
    EXPECT_NE(xml.find("<Ratio>INF</Ratio>"), std::string::npos) << xml;
    EXPECT_EQ(xml.find("Infinity"), std::string::npos) << xml;

    EXPECT_EQ(serializer.Deserialize(xml).Ratio, std::numeric_limits<float>::infinity());

    value.Ratio = -std::numeric_limits<float>::infinity();
    EXPECT_NE(serializer.Serialize(value).find("<Ratio>-INF</Ratio>"), std::string::npos);
    EXPECT_EQ(serializer.Deserialize(serializer.Serialize(value)).Ratio,
              -std::numeric_limits<float>::infinity());
}

// ===============================================================================================
// Escaping. A save file records player- and content-authored text: RolePlayingGame's
// SaveGameDescription.ChapterName is a quest name, and its Description is a formatted DateTime.
// A quest called "Smith & Son" must survive, and must not corrupt the document.
// ===============================================================================================

TEST(XmlLexicalFormTests, MarkupCharactersInTextAreEscapedAndRoundTrip) {
    XmlSerializer<TextHolder> serializer;

    const struct {
        const char* label;
        std::string text;
    } cases[] = {
        {"ampersand", "Smith & Son"},
        {"less-than", "HP < 10"},
        {"greater-than", "HP > 90"},
        {"quotes", "the \"Lost\" Sword"},
        {"apostrophe", "Brom's Blade"},
        {"all at once", "<a> & \"b\" & 'c' </a>"},
        {"entity-looking text", "&amp; is not an ampersand here"},
        {"windows path", "content\\levels\\level1"},
        {"newline", "line one\nline two"},
    };

    for (const auto& testCase : cases) {
        TextHolder value;
        value.Value = testCase.text;
        const std::string xml = serializer.Serialize(value);

        // The document must still be well-formed -- a raw '&' or '<' in the text would make it
        // unparseable, and the round trip is what proves it was escaped rather than dropped.
        const TextHolder back = serializer.Deserialize(xml);
        EXPECT_EQ(back.Value, testCase.text) << testCase.label << " -- emitted: " << xml;
    }
}

TEST(XmlLexicalFormTests, RawAmpersandInTextIsActuallyEscapedOnTheWire) {
    XmlSerializer<TextHolder> serializer;
    TextHolder value;
    value.Value = "Smith & Son";

    const std::string xml = serializer.Serialize(value);

    // Not merely "it round-tripped": the wire form must carry the entity, because that is what
    // makes the file loadable by the original .NET game rather than only by us.
    EXPECT_NE(xml.find("<Value>Smith &amp; Son</Value>"), std::string::npos) << xml;
}

TEST(XmlLexicalFormTests, NonAsciiTextSurvives) {
    XmlSerializer<TextHolder> serializer;
    TextHolder value;
    value.Value = "Přílišně žluťoučký kůň — 龍 — emoji: \xF0\x9F\x97\xA1";

    const TextHolder back = serializer.Deserialize(serializer.Serialize(value));
    EXPECT_EQ(back.Value, value.Value);
}

TEST(XmlLexicalFormTests, EmptyStringRoundTrips) {
    XmlSerializer<TextHolder> serializer;
    TextHolder empty;
    empty.Value = "";
    EXPECT_EQ(serializer.Deserialize(serializer.Serialize(empty)).Value, "");
}

/**
 * A **whitespace-only** string does not survive a round trip, and this pins that as measured
 * behaviour rather than leaving it to be discovered.
 *
 * Localised with `build-probe/xml_probe_whitespace_text.cpp`: the write side is correct and
 * emits `<Value> </Value>`; the **parser** drops it. `XmlDocument::LoadXml` hands back an
 * element with no children at all (`getHasChildNodesProperty() == false`,
 * `OuterXml == "<Value/>"`), because tinyxml2 discards a text node that is entirely whitespace.
 * Real .NET preserves it, so this is a genuine deviation -- in `modules/xml` and its vendored
 * parser, not in this module, which never sees the text node.
 *
 * Blast radius, checked rather than assumed: nothing in the three samples' save or content
 * routes stores a whitespace-only string. `SaveGameDescription.Description` is a formatted
 * `DateTime`, `ChapterName` is a quest name, and every other reachable string is an asset name
 * or an entity name. Surrounding whitespace on real content is unaffected -- `"  a  "` comes
 * back intact, asserted below -- so only the all-whitespace case is lost.
 *
 * Recorded in `docs/XmlSerializationScope.md`. If a route ever needs it, the fix belongs in the
 * Xml module's parse settings, with its own ticket.
 */
TEST(XmlLexicalFormTests, WhitespaceOnlyString_IsLostByTheParser_KnownDeviation) {
    XmlSerializer<TextHolder> serializer;

    TextHolder spaces;
    spaces.Value = " ";

    // The writer is not at fault: the space reaches the wire.
    const std::string xml = serializer.Serialize(spaces);
    EXPECT_NE(xml.find("<Value> </Value>"), std::string::npos) << xml;

    // The reader loses it. Asserted, so a future parser change that fixes this fails here and
    // gets noticed instead of silently altering behaviour.
    EXPECT_EQ(serializer.Deserialize(xml).Value, "");

    // Whitespace *around* real content is preserved, which is what bounds the deviation.
    TextHolder padded;
    padded.Value = "  a  ";
    EXPECT_EQ(serializer.Deserialize(serializer.Serialize(padded)).Value, "  a  ");
}
