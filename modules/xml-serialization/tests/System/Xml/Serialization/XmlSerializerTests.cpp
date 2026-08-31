// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// SAMPLES-DEC-008 vertical slice. The three types below are transcribed field-for-field from
// the real XNA sources (not invented shapes):
//   - SaveGameDescriptionData  <- RolePlayingGame/RolePlayingGame/Session/SaveGameDescription.cs
//     (three public string fields, no nesting -- the simplest real call site).
//   - EntityData/EntityListData <- ShipGame/ShipGame/EntityList.cs
//     (a public string + a 16-float value type, inside a List<T> field -- exercises value-type
//     flattening and a named collection field in the same shape ShipGame's own EntityList.Save/
//     Load use).
// A root-level List<T> (RolePlayingGame's `new XmlSerializer(typeof(List<...>))` call sites) is
// exercised separately below with a minimal item type.
#include <gtest/gtest.h>

#include "System/Xml/Serialization/XmlSerializer.hpp"

using System::Xml::Serialization::XmlSerializer;

namespace {

// --- SaveGameDescription: RolePlayingGame/RolePlayingGame/Session/SaveGameDescription.cs -----

struct SaveGameDescriptionData {
    std::string FileName;
    std::string ChapterName;
    std::string Description;

    SHARP_XML_SERIALIZABLE(SaveGameDescriptionData, "SaveGameDescription",
                            SHARP_XML_M(SaveGameDescriptionData, FileName),
                            SHARP_XML_M(SaveGameDescriptionData, ChapterName),
                            SHARP_XML_M(SaveGameDescriptionData, Description))
};

// --- ShipGame's Entity/EntityList: ShipGame/ShipGame/EntityList.cs ----------------------------

struct MatrixData {
    float M11 = 0, M12 = 0, M13 = 0, M14 = 0;
    float M21 = 0, M22 = 0, M23 = 0, M24 = 0;
    float M31 = 0, M32 = 0, M33 = 0, M34 = 0;
    float M41 = 0, M42 = 0, M43 = 0, M44 = 0;

    SHARP_XML_SERIALIZABLE(MatrixData, "Matrix",
                            SHARP_XML_M(MatrixData, M11), SHARP_XML_M(MatrixData, M12),
                            SHARP_XML_M(MatrixData, M13), SHARP_XML_M(MatrixData, M14),
                            SHARP_XML_M(MatrixData, M21), SHARP_XML_M(MatrixData, M22),
                            SHARP_XML_M(MatrixData, M23), SHARP_XML_M(MatrixData, M24),
                            SHARP_XML_M(MatrixData, M31), SHARP_XML_M(MatrixData, M32),
                            SHARP_XML_M(MatrixData, M33), SHARP_XML_M(MatrixData, M34),
                            SHARP_XML_M(MatrixData, M41), SHARP_XML_M(MatrixData, M42),
                            SHARP_XML_M(MatrixData, M43), SHARP_XML_M(MatrixData, M44))

    static MatrixData Identity() {
        MatrixData m;
        m.M11 = m.M22 = m.M33 = m.M44 = 1.0f;
        return m;
    }

    bool operator==(const MatrixData&) const = default;
};

struct EntityData {
    std::string name;
    MatrixData transform;

    SHARP_XML_SERIALIZABLE(EntityData, "Entity", SHARP_XML_M(EntityData, name), SHARP_XML_M(EntityData, transform))

    bool operator==(const EntityData&) const = default;
};

struct EntityListData {
    std::vector<EntityData> entities;

    SHARP_XML_SERIALIZABLE(EntityListData, "EntityList", SHARP_XML_M(EntityListData, entities))
};

// --- A minimal item type for the root-level List<T> call sites --------------------------------

struct ModifiedChestEntryData {
    std::string chestName;
    bool isTaken = false;

    SHARP_XML_SERIALIZABLE(ModifiedChestEntryData, "ModifiedChestEntry",
                            SHARP_XML_M(ModifiedChestEntryData, chestName),
                            SHARP_XML_M(ModifiedChestEntryData, isTaken))

    bool operator==(const ModifiedChestEntryData&) const = default;
};

// A friend function cannot be *defined* inside a local class, so the negative-control type
// (below) must live at namespace scope like every other registered type here -- not inside the
// TEST body it's used from.
struct SwappedOrderData {
    std::string FileName;
    std::string ChapterName;
    std::string Description;
    SHARP_XML_SERIALIZABLE(SwappedOrderData, "SaveGameDescription",
                            SHARP_XML_M(SwappedOrderData, ChapterName),  // planted: swapped with FileName
                            SHARP_XML_M(SwappedOrderData, FileName), SHARP_XML_M(SwappedOrderData, Description))
};

}  // namespace

// ===============================================================================================
// SaveGameDescription: exact wire-format pin (element names, order, root namespaces), then a
// round trip. The exact-string test is the one a name/order/namespace regression can actually
// fail; the round trip alone could not (see the negative-control test below, which proves it).
// ===============================================================================================

TEST(XmlSerializerTests, SaveGameDescription_SerializesToExactExpectedWire) {
    SaveGameDescriptionData value{"save1.sav", "Chapter 2", "12:34 elapsed"};
    XmlSerializer<SaveGameDescriptionData> serializer;

    std::string xml = serializer.Serialize(value);

    const std::string expected =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<SaveGameDescription xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">"
        "<FileName>save1.sav</FileName>"
        "<ChapterName>Chapter 2</ChapterName>"
        "<Description>12:34 elapsed</Description>"
        "</SaveGameDescription>";
    EXPECT_EQ(xml, expected);
}

TEST(XmlSerializerTests, SaveGameDescription_RoundTrips) {
    SaveGameDescriptionData value{"save1.sav", "Chapter 2", "12:34 elapsed"};
    XmlSerializer<SaveGameDescriptionData> serializer;

    SaveGameDescriptionData back = serializer.Deserialize(serializer.Serialize(value));

    EXPECT_EQ(back.FileName, value.FileName);
    EXPECT_EQ(back.ChapterName, value.ChapterName);
    EXPECT_EQ(back.Description, value.Description);
}

// A negative control (the test-quality rule this project holds itself to): prove the exact-wire
// test above can actually fail, by planting the exact defect class this module is meant to
// catch -- two members swapped, which is indistinguishable from correct if both are read back
// into the same struct shape and only round-trip equality is asserted.
TEST(XmlSerializerTests, NegativeControl_SwappedFieldOrder_FailsTheExactWireAssertion) {
    SwappedOrderData value{"save1.sav", "Chapter 2", "12:34 elapsed"};
    XmlSerializer<SwappedOrderData> serializer;

    std::string xml = serializer.Serialize(value);

    const std::string wireCorrectOrder =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<SaveGameDescription xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">"
        "<FileName>save1.sav</FileName>"
        "<ChapterName>Chapter 2</ChapterName>"
        "<Description>12:34 elapsed</Description>"
        "</SaveGameDescription>";

    // Measured: this is exactly the mismatch a real order regression would produce. Round-trip
    // (Deserialize(Serialize(x)) == x) would NOT have caught it, because deserialization looks
    // members up by name, not position -- which is why the exact-wire test above, not just the
    // round trip, is the one worth keeping.
    EXPECT_NE(xml, wireCorrectOrder);
}

// ===============================================================================================
// ShipGame's Entity/EntityList: value-type flattening (Matrix -> 16 child elements, in
// declaration order) and a named collection field.
// ===============================================================================================

TEST(XmlSerializerTests, Entity_FlattensMatrixToSixteenOrderedElements) {
    EntityData entity;
    entity.name = "carrier_01";
    entity.transform = MatrixData::Identity();
    entity.transform.M41 = 200.5f;
    entity.transform.M42 = -75.25f;
    entity.transform.M43 = 12.0f;

    XmlSerializer<EntityData> serializer;
    std::string xml = serializer.Serialize(entity);

    const std::string expected =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<Entity xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">"
        "<name>carrier_01</name>"
        "<transform>"
        "<M11>1</M11><M12>0</M12><M13>0</M13><M14>0</M14>"
        "<M21>0</M21><M22>1</M22><M23>0</M23><M24>0</M24>"
        "<M31>0</M31><M32>0</M32><M33>1</M33><M34>0</M34>"
        "<M41>200.5</M41><M42>-75.25</M42><M43>12</M43><M44>1</M44>"
        "</transform>"
        "</Entity>";
    EXPECT_EQ(xml, expected);

    EntityData back = serializer.Deserialize(xml);
    EXPECT_TRUE(back == entity);
}

TEST(XmlSerializerTests, EntityList_RoundTripsAMultiEntityCollection) {
    EntityListData list;
    EntityData a;
    a.name = "carrier_01";
    a.transform = MatrixData::Identity();
    EntityData b;
    b.name = "fighter_07";
    b.transform = MatrixData::Identity();
    b.transform.M41 = 10.0f;
    list.entities = {a, b};

    XmlSerializer<EntityListData> serializer;
    std::string xml = serializer.Serialize(list);

    // The collection field is wrapped by its own field name ("entities"), each item named by
    // the item type's registered root name ("Entity") -- XNA's default List<T> field behavior,
    // distinct from the ArrayOf-prefixed name a *root-level* List<T> gets (tested below).
    EXPECT_NE(xml.find("<entities><Entity>"), std::string::npos);
    EXPECT_EQ(xml.find("ArrayOf"), std::string::npos);

    EntityListData back = serializer.Deserialize(xml);
    ASSERT_EQ(back.entities.size(), 2u);
    EXPECT_TRUE(back.entities[0] == a);
    EXPECT_TRUE(back.entities[1] == b);
}

// ===============================================================================================
// Root-level List<T>: RolePlayingGame's `new XmlSerializer(typeof(List<ModifiedChestEntry>))`
// call sites (Session.cs lines ~1733-1868 in the real source). .NET names the root
// "ArrayOf" + the item type's name when List<T>'s T is not itself generic.
// ===============================================================================================

TEST(XmlSerializerTests, RootLevelList_NamesRootArrayOfItemType) {
    std::vector<ModifiedChestEntryData> entries{{"chest_north", true}, {"chest_south", false}};
    XmlSerializer<std::vector<ModifiedChestEntryData>> serializer;

    std::string xml = serializer.Serialize(entries);

    const std::string expected =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<ArrayOfModifiedChestEntry xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">"
        "<ModifiedChestEntry><chestName>chest_north</chestName><isTaken>true</isTaken></ModifiedChestEntry>"
        "<ModifiedChestEntry><chestName>chest_south</chestName><isTaken>false</isTaken></ModifiedChestEntry>"
        "</ArrayOfModifiedChestEntry>";
    EXPECT_EQ(xml, expected);

    std::vector<ModifiedChestEntryData> back = serializer.Deserialize(xml);
    ASSERT_EQ(back.size(), 2u);
    EXPECT_TRUE(back[0] == entries[0]);
    EXPECT_TRUE(back[1] == entries[1]);
}

TEST(XmlSerializerTests, RootLevelList_EmptyListRoundTrips) {
    std::vector<ModifiedChestEntryData> entries;
    XmlSerializer<std::vector<ModifiedChestEntryData>> serializer;

    std::vector<ModifiedChestEntryData> back = serializer.Deserialize(serializer.Serialize(entries));
    EXPECT_TRUE(back.empty());
}

// ===============================================================================================
// A missing required element is a load failure, not a silently default-valued field -- matching
// .NET's own behavior for a non-optional member (RolePlayingGame's save types declare no
// `[XmlElement(IsNullable=...)]`/optional annotations, so every member here is required).
// ===============================================================================================

TEST(XmlSerializerTests, Deserialize_MissingElement_ThrowsXmlException) {
    XmlSerializer<SaveGameDescriptionData> serializer;
    const std::string xmlMissingDescription =
        "<SaveGameDescription><FileName>a</FileName><ChapterName>b</ChapterName></SaveGameDescription>";

    EXPECT_THROW(serializer.Deserialize(xmlMissingDescription), System::Xml::XmlException);
}
