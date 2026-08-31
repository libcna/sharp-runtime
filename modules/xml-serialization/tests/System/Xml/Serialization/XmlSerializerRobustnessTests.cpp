// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// SAMPLES-DEC-008: what happens to input the loader did not write.
//
// A save file is read from disk after an arbitrary interval, possibly truncated by a crash
// mid-write, possibly hand-edited, possibly from a different game version. Every one of these
// must produce a diagnosable failure or a defined value -- never a crash, and never silent
// garbage that the game then renders.
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "System/Xml/Serialization/XmlSerializer.hpp"
#include "System/Xml/XmlException.hpp"

using System::Xml::Serialization::XmlSerializer;

namespace {

struct Item {
    std::string Name;
    std::int32_t Count = 0;
    SHARP_XML_SERIALIZABLE(Item, "Item", SHARP_XML_M(Item, Name), SHARP_XML_M(Item, Count))
};

struct Container {
    std::string Title;
    std::vector<Item> Items;
    SHARP_XML_SERIALIZABLE(Container, "Container", SHARP_XML_M(Container, Title),
                            SHARP_XML_M(Container, Items))
};

}  // namespace

TEST(XmlSerializerRobustnessTests, MalformedDocumentsThrowRatherThanCrash) {
    XmlSerializer<Container> serializer;

    const struct {
        const char* label;
        const char* xml;
    } cases[] = {
        {"empty input", ""},
        {"only whitespace", "   \n  "},
        {"unclosed root", "<Container><Title>a</Title>"},
        {"mismatched tags", "<Container><Title>a</Wrong></Container>"},
        {"truncated mid-tag", "<Container><Tit"},
        {"declaration only", "<?xml version=\"1.0\"?>"},
        {"not xml at all", "this is not xml"},
        {"undeclared entity", "<Container><Title>&nosuch;</Title></Container>"},
    };

    for (const auto& testCase : cases) {
        EXPECT_THROW(
            {
                Container ignored = serializer.Deserialize(testCase.xml);
                (void)ignored;
            },
            System::Xml::XmlException)
            << testCase.label;
    }
}

/**
 * A bare `&` is accepted and passed through as literal text, where .NET's `XmlReader` would
 * reject the document. Measured with `build-probe/xml_probe_entity_leniency.cpp`, pinned here
 * because it is a deviation and should not be discovered by surprise.
 *
 * The direction is the safe one for a loader: this reads *more* files than .NET, never fewer,
 * and it never turns valid input into wrong data -- real references still resolve correctly
 * (`&amp;` to `&`, `&#65;` to `A`), and an entity that looks declared but is not still throws.
 *
 * The writer is unaffected: it escapes unconditionally, so this module never emits a bare `&`.
 * Reading a hand-edited file with one and writing it back therefore repairs it, which the last
 * assertion shows.
 */
TEST(XmlSerializerRobustnessTests, BareAmpersandIsAcceptedAsLiteralText_KnownLeniency) {
    XmlSerializer<Item> serializer;

    EXPECT_EQ(serializer.Deserialize("<Item><Name>a & b</Name><Count>1</Count></Item>").Name, "a & b");
    EXPECT_EQ(serializer.Deserialize("<Item><Name>&amp</Name><Count>1</Count></Item>").Name, "&amp");

    // Real references are unaffected and still resolve.
    EXPECT_EQ(serializer.Deserialize("<Item><Name>&amp;</Name><Count>1</Count></Item>").Name, "&");
    EXPECT_EQ(serializer.Deserialize("<Item><Name>&#65;</Name><Count>1</Count></Item>").Name, "A");

    // Writing it back produces a well-formed document -- the bare ampersand is repaired.
    const Item loaded = serializer.Deserialize("<Item><Name>a & b</Name><Count>1</Count></Item>");
    const std::string rewritten = serializer.Serialize(loaded);
    EXPECT_NE(rewritten.find("a &amp; b"), std::string::npos) << rewritten;
    EXPECT_EQ(serializer.Deserialize(rewritten).Name, "a & b");
}

/**
 * A well-formed document whose root is some *other* type loads as a default-valued instance
 * rather than throwing.
 *
 * That is .NET's behaviour for a member-name mismatch and it is the right one here: the
 * alternative is refusing a save whose format merely grew a field. It is asserted explicitly
 * because it is a decision, not an accident -- a caller that needs to reject a foreign document
 * checks the root name first, which is what `RootElementName()` is for.
 */
TEST(XmlSerializerRobustnessTests, WrongRootElement_YieldsDefaultsNotAnException) {
    XmlSerializer<Container> serializer;
    const Container value = serializer.Deserialize("<SomethingElse><Other>1</Other></SomethingElse>");

    EXPECT_EQ(value.Title, "");
    EXPECT_TRUE(value.Items.empty());
}

TEST(XmlSerializerRobustnessTests, WrongTypeInNumericField_Throws) {
    XmlSerializer<Item> serializer;

    // A count that is not a number must be reported, not silently read as zero: a zero item
    // count is a legitimate value, so guessing here would hide a corrupt save.
    EXPECT_THROW((void)serializer.Deserialize("<Item><Name>a</Name><Count>banana</Count></Item>"),
                  std::exception);
    EXPECT_THROW((void)serializer.Deserialize("<Item><Name>a</Name><Count></Count></Item>"),
                  std::exception);
}

TEST(XmlSerializerRobustnessTests, ItemsOfTheWrongNameInsideACollectionAreSkipped) {
    XmlSerializer<Container> serializer;
    const Container value = serializer.Deserialize(
        "<Container><Title>t</Title><Items>"
        "<Item><Name>keep</Name><Count>1</Count></Item>"
        "<NotAnItem><Name>drop</Name></NotAnItem>"
        "<Item><Name>keep2</Name><Count>2</Count></Item>"
        "</Items></Container>");

    // Two kept, the foreign element ignored -- a version of the game that added a sibling
    // element must not make the list unreadable.
    ASSERT_EQ(value.Items.size(), 2u);
    EXPECT_EQ(value.Items[0].Name, "keep");
    EXPECT_EQ(value.Items[1].Name, "keep2");
}

TEST(XmlSerializerRobustnessTests, DeeplyNestedInputDoesNotBlowTheStack) {
    // Not a shape this module produces, but a shape a hostile or corrupt file can contain.
    // The types here are shallow, so the nesting is simply unrecognised -- what matters is that
    // parsing terminates and reports rather than recursing without bound.
    std::string deep;
    const int depth = 5000;
    for (int i = 0; i < depth; ++i) deep += "<n>";
    for (int i = 0; i < depth; ++i) deep += "</n>";

    XmlSerializer<Container> serializer;
    try {
        Container value = serializer.Deserialize("<Container>" + deep + "</Container>");
        // If it parsed, the registered members are simply absent and stay at their defaults.
        EXPECT_EQ(value.Title, "");
    } catch (const System::Xml::XmlException&) {
        SUCCEED() << "reported a depth limit, which is also a defined outcome";
    }
}

TEST(XmlSerializerRobustnessTests, VeryLongTextValueSurvives) {
    XmlSerializer<Item> serializer;
    Item value;
    value.Name = std::string(1'000'000, 'x');
    value.Count = 7;

    const Item back = serializer.Deserialize(serializer.Serialize(value));
    EXPECT_EQ(back.Name.size(), 1'000'000u);
    EXPECT_EQ(back.Count, 7);
}

/**
 * Scale, measured rather than assumed (`build-probe/xml_probe_scale.cpp`, -O2): reading a list
 * of three-member entries costs ~23 us/entry at 100 entries and ~29 us/entry at 20,000, so the
 * per-member child scan is effectively linear in document size, not quadratic. A 40-member
 * element repeated 2,000 times reads in ~480 ms.
 *
 * This test keeps a much smaller version in the suite so a future change that makes the lookup
 * quadratic shows up as a timeout rather than as a slow game.
 */
TEST(XmlSerializerRobustnessTests, LargeCollectionRoundTrips) {
    Container value;
    value.Title = "big";
    value.Items.reserve(5000);
    for (int i = 0; i < 5000; ++i) {
        value.Items.push_back({"item" + std::to_string(i), i});
    }

    XmlSerializer<Container> serializer;
    const Container back = serializer.Deserialize(serializer.Serialize(value));

    ASSERT_EQ(back.Items.size(), 5000u);
    EXPECT_EQ(back.Items[0].Name, "item0");
    EXPECT_EQ(back.Items[4999].Name, "item4999");
    EXPECT_EQ(back.Items[4999].Count, 4999);
}
