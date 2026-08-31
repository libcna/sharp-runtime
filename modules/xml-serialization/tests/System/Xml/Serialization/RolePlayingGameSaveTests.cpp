// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// SAMPLES-DEC-008 / SAMPLE-070: the RolePlayingGame save route, end to end.
//
// **This is the call-site shape that matters most, and it is not the one the other test files
// exercise.** Sixteen of the twenty `XmlSerializer` uses in
// `RolePlayingGame/Session/Session.cs` serialize into an *already open* `XmlWriter`:
//
//     xmlWriter.WriteStartElement("rolePlayingGameSaveData");
//     xmlWriter.WriteStartElement("mapData");
//     xmlWriter.WriteElementString("mapContentName", TileEngine.Map.AssetName);
//     new XmlSerializer(typeof(PlayerPosition)).Serialize(xmlWriter, ...);
//     new XmlSerializer(typeof(List<WorldEntry<Chest>>)).Serialize(xmlWriter, ...);
//     new XmlSerializer(typeof(List<WorldEntry<FixedCombat>>)).Serialize(xmlWriter, ...);
//     new XmlSerializer(typeof(List<WorldEntry<Player>>)).Serialize(xmlWriter, ...);
//     new XmlSerializer(typeof(List<ModifiedChestEntry>)).Serialize(xmlWriter, ...);
//     xmlWriter.WriteEndElement();
//
// so several serialized objects share one document instead of each producing its own. The test
// below reproduces that document with `SerializeInto`/`DeserializeFrom`, and reads it back
// through the same route the game's Load does.
//
// The types are transcribed from the real sources: PlayerPosition.cs, ModifiedChestEntry.cs,
// PartySaveData.cs, WorldEntry.cs/MapEntry.cs/ContentEntry.cs.
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "System/Xml/Serialization/XmlSerializer.hpp"
#include "System/Xml/XmlDocument.hpp"

using System::Xml::Serialization::XmlSerializer;

namespace {

// C# writes `public Direction Direction = Direction.South;` -- a member named after its own
// type. C++ rejects that ([-Wchanges-meaning]), so the *type* is renamed here and the *member*
// keeps the name `Direction`. That is the one that matters: the XML element name comes from the
// member, so the wire form is unchanged. A real port of these types will hit the same rule.
enum class FacingDirection { North, East, South, West };

SHARP_XML_ENUM(FacingDirection, SHARP_XML_E(FacingDirection, North),
                SHARP_XML_E(FacingDirection, East), SHARP_XML_E(FacingDirection, South),
                SHARP_XML_E(FacingDirection, West))

struct Point {
    std::int32_t X = 0;
    std::int32_t Y = 0;
    SHARP_XML_SERIALIZABLE(Point, "Point", SHARP_XML_M(Point, X), SHARP_XML_M(Point, Y))
    bool operator==(const Point&) const = default;
};

struct Vector2 {
    float X = 0;
    float Y = 0;
    SHARP_XML_SERIALIZABLE(Vector2, "Vector2", SHARP_XML_M(Vector2, X), SHARP_XML_M(Vector2, Y))
    bool operator==(const Vector2&) const = default;
};

// RolePlayingGame/TileEngine/PlayerPosition.cs. ScreenPosition is a get-only computed property
// and therefore not serialized by .NET either -- it is simply not registered.
struct PlayerPosition {
    Point TilePosition;
    Vector2 TileOffset;
    FacingDirection Direction = FacingDirection::South;

    SHARP_XML_SERIALIZABLE(PlayerPosition, "PlayerPosition", SHARP_XML_M(PlayerPosition, TilePosition),
                            SHARP_XML_M(PlayerPosition, TileOffset),
                            SHARP_XML_M(PlayerPosition, Direction))
    bool operator==(const PlayerPosition&) const = default;
};

// RolePlayingGame/Session/ModifiedChestEntry.cs
struct ModifiedChestEntry {
    std::string ChestName;
    std::vector<std::string> RemovedEntries;
    SHARP_XML_SERIALIZABLE(ModifiedChestEntry, "ModifiedChestEntry",
                            SHARP_XML_M(ModifiedChestEntry, ChestName),
                            SHARP_XML_M(ModifiedChestEntry, RemovedEntries))
    bool operator==(const ModifiedChestEntry&) const = default;
};

// ContentEntry<T> -> MapEntry<T> -> WorldEntry<T>, flattened per instantiation. The registered
// root name is .NET's name for the closed generic, so a list of these becomes
// <ArrayOfWorldEntryOfChest> with no name-mangling machinery.
struct WorldEntryOfChest {
    std::string ContentName;
    Point MapPosition;
    FacingDirection Direction = FacingDirection::South;
    std::string MapContentName;

    SHARP_XML_SERIALIZABLE(WorldEntryOfChest, "WorldEntryOfChest",
                            SHARP_XML_M(WorldEntryOfChest, ContentName),
                            SHARP_XML_M(WorldEntryOfChest, MapPosition),
                            SHARP_XML_M(WorldEntryOfChest, Direction),
                            SHARP_XML_M(WorldEntryOfChest, MapContentName))
    bool operator==(const WorldEntryOfChest&) const = default;
};

struct WorldEntryOfPlayer {
    std::string ContentName;
    Point MapPosition;
    FacingDirection Direction = FacingDirection::South;
    std::string MapContentName;

    SHARP_XML_SERIALIZABLE(WorldEntryOfPlayer, "WorldEntryOfPlayer",
                            SHARP_XML_M(WorldEntryOfPlayer, ContentName),
                            SHARP_XML_M(WorldEntryOfPlayer, MapPosition),
                            SHARP_XML_M(WorldEntryOfPlayer, Direction),
                            SHARP_XML_M(WorldEntryOfPlayer, MapContentName))
    bool operator==(const WorldEntryOfPlayer&) const = default;
};

// RolePlayingGame/Session/PlayerSaveData.cs + PartySaveData.cs
struct StatisticsValue {
    std::int32_t HealthPoints = 0;
    std::int32_t MagicPoints = 0;
    std::int32_t PhysicalOffense = 0;
    std::int32_t PhysicalDefense = 0;

    SHARP_XML_SERIALIZABLE(StatisticsValue, "StatisticsValue",
                            SHARP_XML_M(StatisticsValue, HealthPoints),
                            SHARP_XML_M(StatisticsValue, MagicPoints),
                            SHARP_XML_M(StatisticsValue, PhysicalOffense),
                            SHARP_XML_M(StatisticsValue, PhysicalDefense))
    bool operator==(const StatisticsValue&) const = default;
};

struct PlayerSaveData {
    std::string assetName;
    std::int32_t characterLevel = 0;
    std::int32_t experience = 0;
    std::vector<std::string> equipmentAssetNames;
    StatisticsValue statisticsModifiers;

    SHARP_XML_SERIALIZABLE(PlayerSaveData, "PlayerSaveData", SHARP_XML_M(PlayerSaveData, assetName),
                            SHARP_XML_M(PlayerSaveData, characterLevel),
                            SHARP_XML_M(PlayerSaveData, experience),
                            SHARP_XML_M(PlayerSaveData, equipmentAssetNames),
                            SHARP_XML_M(PlayerSaveData, statisticsModifiers))
    bool operator==(const PlayerSaveData&) const = default;
};

struct ContentEntryOfGear {
    std::string ContentName;
    std::int32_t Count = 0;
    SHARP_XML_SERIALIZABLE(ContentEntryOfGear, "ContentEntryOfGear",
                            SHARP_XML_M(ContentEntryOfGear, ContentName),
                            SHARP_XML_M(ContentEntryOfGear, Count))
    bool operator==(const ContentEntryOfGear&) const = default;
};

struct PartySaveData {
    std::vector<PlayerSaveData> players;
    std::vector<ContentEntryOfGear> inventory;
    std::int32_t partyGold = 0;
    std::vector<std::string> monsterKillNames;
    std::vector<std::int32_t> monsterKillCounts;

    SHARP_XML_SERIALIZABLE(PartySaveData, "PartySaveData", SHARP_XML_M(PartySaveData, players),
                            SHARP_XML_M(PartySaveData, inventory),
                            SHARP_XML_M(PartySaveData, partyGold),
                            SHARP_XML_M(PartySaveData, monsterKillNames),
                            SHARP_XML_M(PartySaveData, monsterKillCounts))
};

/** @brief One populated save, so the assertions below compare real values rather than defaults. */
struct SaveFixture {
    std::string mapContentName = "Maps/Village";
    PlayerPosition playerPosition;
    std::vector<WorldEntryOfChest> removedMapChests;
    std::vector<WorldEntryOfPlayer> removedMapPlayerNpcs;
    std::vector<ModifiedChestEntry> modifiedMapChests;
    PartySaveData party;

    SaveFixture() {
        playerPosition.TilePosition = {14, 22};
        playerPosition.TileOffset = {0.5f, -0.25f};
        playerPosition.Direction = FacingDirection::East;

        removedMapChests.push_back({"Chests/VillageChest", {3, 4}, FacingDirection::North, "Maps/Village"});
        removedMapChests.push_back({"Chests/CaveChest", {9, 1}, FacingDirection::West, "Maps/Cave"});

        removedMapPlayerNpcs.push_back({"Players/Hilda", {7, 7}, FacingDirection::South, "Maps/Village"});

        modifiedMapChests.push_back({"Chests/VillageChest", {"Gear/Potion", "Gear/Sword"}});

        PlayerSaveData hero;
        hero.assetName = "Players/Brom";
        hero.characterLevel = 6;
        hero.experience = 1450;
        hero.equipmentAssetNames = {"Gear/IronSword", "Gear/LeatherArmor"};
        hero.statisticsModifiers = {12, 4, 3, 2};
        party.players.push_back(hero);

        party.inventory.push_back({"Gear/Potion", 5});
        party.partyGold = 1250;
        party.monsterKillNames = {"Skeleton", "Bandit"};
        party.monsterKillCounts = {3, 7};
    }
};

/** @brief Builds the document Session.Save writes, using the nested-serialization API. */
[[nodiscard]] std::string WriteSaveDocument(const SaveFixture& save) {
    System::Xml::XmlDocument doc;
    doc.AppendChild(doc.CreateXmlDeclaration("1.0", "", ""));

    System::Xml::XmlElement* root = doc.CreateElement("rolePlayingGameSaveData");
    doc.AppendChild(root);

    System::Xml::XmlElement* mapData = doc.CreateElement("mapData");
    root->AppendChild(mapData);

    System::Xml::XmlElement* mapContentName = doc.CreateElement("mapContentName");
    mapContentName->setInnerTextProperty(save.mapContentName);
    mapData->AppendChild(mapContentName);

    XmlSerializer<PlayerPosition>{}.SerializeInto(doc, mapData, save.playerPosition);
    XmlSerializer<std::vector<WorldEntryOfChest>>{}.SerializeInto(doc, mapData, save.removedMapChests);
    XmlSerializer<std::vector<WorldEntryOfPlayer>>{}.SerializeInto(doc, mapData,
                                                                     save.removedMapPlayerNpcs);
    XmlSerializer<std::vector<ModifiedChestEntry>>{}.SerializeInto(doc, mapData, save.modifiedMapChests);

    XmlSerializer<PartySaveData>{}.SerializeInto(doc, root, save.party);

    return doc.getOuterXmlProperty();
}

}  // namespace

TEST(RolePlayingGameSaveTests, NestedSerializationProducesTheSaveDocumentShape) {
    const std::string xml = WriteSaveDocument(SaveFixture{});

    // The nesting itself: one document, several serialized objects inside it.
    EXPECT_NE(xml.find("<rolePlayingGameSaveData>"), std::string::npos) << xml;
    EXPECT_NE(xml.find("<mapData>"), std::string::npos);
    EXPECT_NE(xml.find("<mapContentName>Maps/Village</mapContentName>"), std::string::npos);

    // Each nested root carries the schema namespaces, as .NET writes them even mid-document.
    EXPECT_NE(xml.find("<PlayerPosition xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">"),
               std::string::npos)
        << xml;

    // .NET's closed-generic names for the two list roots.
    EXPECT_NE(xml.find("<ArrayOfWorldEntryOfChest "), std::string::npos);
    EXPECT_NE(xml.find("<ArrayOfWorldEntryOfPlayer "), std::string::npos);
    EXPECT_NE(xml.find("<ArrayOfModifiedChestEntry "), std::string::npos);

    // Exactly one declaration, at the top -- the nested elements must not add their own.
    EXPECT_TRUE(xml.starts_with("<?xml version=\"1.0\"?>")) << xml.substr(0, 60);
    EXPECT_EQ(xml.find("<?xml", 5), std::string::npos) << "a nested element wrote a second declaration";

    // The enum went out as a name and the primitive lists took XSD item names.
    EXPECT_NE(xml.find("<Direction>East</Direction>"), std::string::npos);
    EXPECT_NE(xml.find("<monsterKillNames><string>Skeleton</string><string>Bandit</string></monsterKillNames>"),
               std::string::npos);
    EXPECT_NE(xml.find("<monsterKillCounts><int>3</int><int>7</int></monsterKillCounts>"),
               std::string::npos);
}

TEST(RolePlayingGameSaveTests, SaveThenLoadRestoresEveryRoute) {
    const SaveFixture original;
    const std::string xml = WriteSaveDocument(original);

    // The Load side: parse the document, then hand each nested element to its serializer --
    // the DOM equivalent of Session.Load's sequential Deserialize(xmlReader) calls.
    System::Xml::XmlDocument doc;
    doc.LoadXml(xml);
    System::Xml::XmlElement* root = doc.getDocumentElementProperty();
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(root->getNameProperty(), "rolePlayingGameSaveData");

    auto childNamed = [](System::Xml::XmlNode* parent, const std::string& name) -> System::Xml::XmlElement* {
        for (System::Xml::XmlNode* child = parent->getFirstChildProperty(); child != nullptr;
             child = child->getNextSiblingProperty()) {
            if (child->getNameProperty() == name) return static_cast<System::Xml::XmlElement*>(child);
        }
        return nullptr;
    };

    System::Xml::XmlElement* mapData = childNamed(root, "mapData");
    ASSERT_NE(mapData, nullptr);
    EXPECT_EQ(childNamed(mapData, "mapContentName")->getInnerTextProperty(), original.mapContentName);

    // RootElementName() is what lets a caller locate a node without re-deriving the ArrayOf rule.
    const PlayerPosition position = XmlSerializer<PlayerPosition>{}.DeserializeFrom(
        childNamed(mapData, XmlSerializer<PlayerPosition>::RootElementName()));
    EXPECT_TRUE(position == original.playerPosition);

    using ChestList = std::vector<WorldEntryOfChest>;
    const ChestList chests =
        XmlSerializer<ChestList>{}.DeserializeFrom(childNamed(mapData, XmlSerializer<ChestList>::RootElementName()));
    ASSERT_EQ(chests.size(), original.removedMapChests.size());
    for (std::size_t i = 0; i < chests.size(); ++i) {
        EXPECT_TRUE(chests[i] == original.removedMapChests[i]) << "chest " << i;
    }

    using NpcList = std::vector<WorldEntryOfPlayer>;
    const NpcList npcs =
        XmlSerializer<NpcList>{}.DeserializeFrom(childNamed(mapData, XmlSerializer<NpcList>::RootElementName()));
    ASSERT_EQ(npcs.size(), original.removedMapPlayerNpcs.size());
    EXPECT_TRUE(npcs[0] == original.removedMapPlayerNpcs[0]);

    using ModifiedList = std::vector<ModifiedChestEntry>;
    const ModifiedList modified = XmlSerializer<ModifiedList>{}.DeserializeFrom(
        childNamed(mapData, XmlSerializer<ModifiedList>::RootElementName()));
    ASSERT_EQ(modified.size(), 1u);
    EXPECT_EQ(modified[0].ChestName, "Chests/VillageChest");
    // The nested List<string> inside a list item -- two levels of collection.
    ASSERT_EQ(modified[0].RemovedEntries.size(), 2u);
    EXPECT_EQ(modified[0].RemovedEntries[0], "Gear/Potion");
    EXPECT_EQ(modified[0].RemovedEntries[1], "Gear/Sword");

    const PartySaveData party = XmlSerializer<PartySaveData>{}.DeserializeFrom(
        childNamed(root, XmlSerializer<PartySaveData>::RootElementName()));
    ASSERT_EQ(party.players.size(), 1u);
    EXPECT_TRUE(party.players[0] == original.party.players[0]);
    ASSERT_EQ(party.inventory.size(), 1u);
    EXPECT_TRUE(party.inventory[0] == original.party.inventory[0]);
    EXPECT_EQ(party.partyGold, 1250);
    EXPECT_EQ(party.monsterKillNames, original.party.monsterKillNames);
    EXPECT_EQ(party.monsterKillCounts, original.party.monsterKillCounts);
}

/**
 * The two WorldEntry instantiations must not be confused for one another.
 *
 * They have identical member shapes and differ only in their registered root name, so a lookup
 * that matched on structure rather than name would silently load the NPC list into the chest
 * list. The save document contains both, which is what makes this checkable.
 */
TEST(RolePlayingGameSaveTests, TwoGenericInstantiationsWithIdenticalShapesStayDistinct) {
    const SaveFixture original;
    const std::string xml = WriteSaveDocument(original);

    System::Xml::XmlDocument doc;
    doc.LoadXml(xml);

    // GetElementsByTagName hands the caller an owned XmlNodeList. Held in unique_ptr because it
    // is not owned by the document -- AddressSanitizer's leak check caught the raw-pointer
    // version of this test, which is exactly what it is there for.
    const std::unique_ptr<System::Xml::XmlNodeList> chestRoots(
        doc.GetElementsByTagName("ArrayOfWorldEntryOfChest"));
    const std::unique_ptr<System::Xml::XmlNodeList> npcRoots(
        doc.GetElementsByTagName("ArrayOfWorldEntryOfPlayer"));
    ASSERT_NE(chestRoots, nullptr);
    ASSERT_NE(npcRoots, nullptr);
    ASSERT_EQ(chestRoots->getCountProperty(), 1);
    ASSERT_EQ(npcRoots->getCountProperty(), 1);

    const auto chests = XmlSerializer<std::vector<WorldEntryOfChest>>{}.DeserializeFrom(
        static_cast<System::Xml::XmlElement*>(chestRoots->Item(0)));
    const auto npcs = XmlSerializer<std::vector<WorldEntryOfPlayer>>{}.DeserializeFrom(
        static_cast<System::Xml::XmlElement*>(npcRoots->Item(0)));

    // Two chests, one NPC: distinguishable counts, so a swap or a merge fails rather than
    // producing plausible-looking data.
    EXPECT_EQ(chests.size(), 2u);
    EXPECT_EQ(npcs.size(), 1u);
    EXPECT_EQ(chests[0].ContentName, "Chests/VillageChest");
    EXPECT_EQ(npcs[0].ContentName, "Players/Hilda");
}
