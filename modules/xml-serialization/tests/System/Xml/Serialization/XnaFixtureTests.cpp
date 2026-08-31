// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// SAMPLES-DEC-008 golden-fixture coverage.
//
// The XNA Game Studio source tree ships files that were produced by the real .NET
// `XmlSerializer` and are loaded at runtime by the games themselves -- ShipGame's
// `level1_spawns.xml` (EntityList), `level1_lights.xml` (LightList), `ship1.xml`/`ship2.xml`,
// Spacewar's `settings.xml`, NetRumble's particle effects and UnitConverter's
// `SupportedUnits.xml`. They are Microsoft's own output, so they are the only source of truth
// available for "is this wire-compatible?" that does not depend on this repository's own
// understanding of the format.
//
// Two kinds of test live here, deliberately:
//
//   1. **Transcribed excerpts, embedded below.** Short structural transcriptions that run
//      everywhere, including CI, with no external checkout. They pin the shapes.
//   2. **The real files, when present.** Pointed at by the `XNA_SAMPLES_ROOT` environment
//      variable (default `/rv/tmp/XNAGameStudio/Samples`), skipped with GTEST_SKIP when the
//      tree is absent. These are the ones that can catch a divergence a transcription would
//      have quietly reproduced, because nobody typed them.
//
// The comparison is deliberately **semantic, not byte-for-byte**: parse the fixture, assert the
// values, re-serialize, parse again, and assert the values are unchanged. Byte equality is not
// the contract and provably cannot be -- `settings.xml` is indented with two spaces and
// `level1_spawns.xml` with four, both genuine XmlSerializer output in the same official tree.
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>
#include <string>
#include <vector>

#include "System/Xml/Serialization/XmlSerializer.hpp"

using System::Xml::Serialization::XmlSerializationOptions;
using System::Xml::Serialization::XmlSerializer;

namespace {

// --- the ShipGame types, transcribed field-for-field from EntityList.cs / LightList.cs -------

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

    bool operator==(const MatrixData&) const = default;
};

struct EntityData {
    std::string name;
    MatrixData transform;
    SHARP_XML_SERIALIZABLE(EntityData, "Entity", SHARP_XML_M(EntityData, name),
                            SHARP_XML_M(EntityData, transform))
    bool operator==(const EntityData&) const = default;
};

struct EntityListData {
    std::vector<EntityData> entities;
    SHARP_XML_SERIALIZABLE(EntityListData, "EntityList", SHARP_XML_M(EntityListData, entities))
};

// ShipGame's Vector3 fields serialize as <X>/<Y>/<Z> -- confirmed against level1_lights.xml.
struct Vector3Data {
    float X = 0, Y = 0, Z = 0;
    SHARP_XML_SERIALIZABLE(Vector3Data, "Vector3", SHARP_XML_M(Vector3Data, X),
                            SHARP_XML_M(Vector3Data, Y), SHARP_XML_M(Vector3Data, Z))
    bool operator==(const Vector3Data&) const = default;
};

struct LightData {
    Vector3Data position;
    float radius = 0;
    Vector3Data color;
    SHARP_XML_SERIALIZABLE(LightData, "Light", SHARP_XML_M(LightData, position),
                            SHARP_XML_M(LightData, radius), SHARP_XML_M(LightData, color))
    bool operator==(const LightData&) const = default;
};

struct LightListData {
    Vector3Data ambient;
    std::vector<LightData> lights;
    SHARP_XML_SERIALIZABLE(LightListData, "LightList", SHARP_XML_M(LightListData, ambient),
                            SHARP_XML_M(LightListData, lights))
};

// --- Spacewar's Settings, transcribed from settings.xml's leading members --------------------

struct Vector2Data {
    float X = 0, Y = 0;
    SHARP_XML_SERIALIZABLE(Vector2Data, "Vector2", SHARP_XML_M(Vector2Data, X),
                            SHARP_XML_M(Vector2Data, Y))
    bool operator==(const Vector2Data&) const = default;
};

struct PlayerShipInfoData {
    Vector2Data StartPosition;
    float StartAngle = 0;
    SHARP_XML_SERIALIZABLE(PlayerShipInfoData, "PlayerShipInfo",
                            SHARP_XML_M(PlayerShipInfoData, StartPosition),
                            SHARP_XML_M(PlayerShipInfoData, StartAngle))
    bool operator==(const PlayerShipInfoData&) const = default;
};

struct WeaponInfoData {
    std::int32_t Cost = 0;
    float Lifetime = 0;
    std::int32_t Max = 0;
    std::int32_t Burst = 0;
    float Acceleration = 0;
    std::int32_t Damage = 0;
    SHARP_XML_SERIALIZABLE(WeaponInfoData, "WeaponInfo", SHARP_XML_M(WeaponInfoData, Cost),
                            SHARP_XML_M(WeaponInfoData, Lifetime), SHARP_XML_M(WeaponInfoData, Max),
                            SHARP_XML_M(WeaponInfoData, Burst),
                            SHARP_XML_M(WeaponInfoData, Acceleration),
                            SHARP_XML_M(WeaponInfoData, Damage))
    bool operator==(const WeaponInfoData&) const = default;
};

struct Vector4Data {
    float X = 0, Y = 0, Z = 0, W = 0;
    SHARP_XML_SERIALIZABLE(Vector4Data, "Vector4", SHARP_XML_M(Vector4Data, X),
                            SHARP_XML_M(Vector4Data, Y), SHARP_XML_M(Vector4Data, Z),
                            SHARP_XML_M(Vector4Data, W))
    bool operator==(const Vector4Data&) const = default;
};

// Settings.ShipLighting -- five Vector4s and a float, inside a C# ARRAY field
// (`ShipLighting[] ShipLights`). .NET gives an array and a List<T> the identical wire form, so
// a port maps either to std::vector and nothing further is needed; the fixture below is the
// proof, since ShipLights really is `ShipLighting[]` in Settings.cs.
struct ShipLightingData {
    Vector4Data Ambient;
    Vector4Data DirectionalDirection;
    Vector4Data DirectionalColor;
    Vector4Data PointPosition;
    Vector4Data PointColor;
    float PointFactor = 0;

    SHARP_XML_SERIALIZABLE(ShipLightingData, "ShipLighting", SHARP_XML_M(ShipLightingData, Ambient),
                            SHARP_XML_M(ShipLightingData, DirectionalDirection),
                            SHARP_XML_M(ShipLightingData, DirectionalColor),
                            SHARP_XML_M(ShipLightingData, PointPosition),
                            SHARP_XML_M(ShipLightingData, PointColor),
                            SHARP_XML_M(ShipLightingData, PointFactor))
    bool operator==(const ShipLightingData&) const = default;
};

// Microsoft.Xna.Framework.Input.Keys, restricted to the sixteen enumerators settings.xml
// actually names. The real enum has around 150; a port registers as many as it needs, and the
// point here is that the fixture's values map by NAME, which is how .NET writes an enum.
enum class KeysData {
    A, D, Delete, Down, E, Insert, Left, LeftControl, LeftShift,
    Q, Right, RightControl, RightShift, S, Up, W,
};

SHARP_XML_ENUM(KeysData, SHARP_XML_E(KeysData, A), SHARP_XML_E(KeysData, D),
                SHARP_XML_E(KeysData, Delete), SHARP_XML_E(KeysData, Down),
                SHARP_XML_E(KeysData, E), SHARP_XML_E(KeysData, Insert),
                SHARP_XML_E(KeysData, Left), SHARP_XML_E(KeysData, LeftControl),
                SHARP_XML_E(KeysData, LeftShift), SHARP_XML_E(KeysData, Q),
                SHARP_XML_E(KeysData, Right), SHARP_XML_E(KeysData, RightControl),
                SHARP_XML_E(KeysData, RightShift), SHARP_XML_E(KeysData, S),
                SHARP_XML_E(KeysData, Up), SHARP_XML_E(KeysData, W))

// Only the members needed to prove the shape; unlisted ones are simply not registered, which
// exercises the same "ignore what you were not told about" path a real partial load takes.
struct SettingsData {
    std::string MediaPath;
    std::string WindowTitle;
    std::int32_t LevelTime = 0;
    float FrictionFactor = 0;
    float ShipRecoveryTime = 0;
    Vector2Data SunPosition;
    std::vector<PlayerShipInfoData> Ships;
    std::vector<WeaponInfoData> Weapons;
    std::vector<ShipLightingData> ShipLights;
    KeysData Player1Start = KeysData::A;
    KeysData Player2Start = KeysData::A;
    KeysData Player2RightTrigger = KeysData::A;

    SHARP_XML_SERIALIZABLE(SettingsData, "Settings", SHARP_XML_M(SettingsData, MediaPath),
                            SHARP_XML_M(SettingsData, WindowTitle),
                            SHARP_XML_M(SettingsData, LevelTime),
                            SHARP_XML_M(SettingsData, FrictionFactor),
                            SHARP_XML_M(SettingsData, ShipRecoveryTime),
                            SHARP_XML_M(SettingsData, SunPosition), SHARP_XML_M(SettingsData, Ships),
                            SHARP_XML_M(SettingsData, Weapons), SHARP_XML_M(SettingsData, ShipLights),
                            SHARP_XML_M(SettingsData, Player1Start),
                            SHARP_XML_M(SettingsData, Player2Start),
                            SHARP_XML_M(SettingsData, Player2RightTrigger))
};

// --- fixture plumbing --------------------------------------------------------------------------

[[nodiscard]] std::string SamplesRoot() {
    if (const char* fromEnvironment = std::getenv("XNA_SAMPLES_ROOT")) return fromEnvironment;
    return "/rv/tmp/XNAGameStudio/Samples";
}

/** @return The file's bytes, or an empty optional-ish empty string if it is not there. */
[[nodiscard]] std::string ReadFileOrEmpty(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/** @brief Strips a UTF-8 BOM, which UnitConverter's SupportedUnits.xml carries. */
[[nodiscard]] std::string WithoutBom(std::string text) {
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    return text;
}

// A friend function cannot be defined inside a local class, so every registered type lives at
// namespace scope. This one exists only for the float-exponent fixture below.
struct DurationHolder {
    float Duration = 0;
    SHARP_XML_SERIALIZABLE(DurationHolder, "ParticleSystem", SHARP_XML_M(DurationHolder, Duration))
};

}  // namespace

// ===============================================================================================
// 1. Transcribed excerpts -- always run.
// ===============================================================================================

// Transcribed from ShipGame/Content/levels/level1/level1_spawns.xml (first entity, four-space
// indentation preserved, so this also proves an indented document parses).
TEST(XnaFixtureTests, ShipGameEntityList_ExcerptParsesAndRoundTrips) {
    const std::string fixture = R"(<?xml version="1.0"?>
<EntityList xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema">
    <entities>
        <Entity>
            <name>spawn0</name>
            <transform>
                <M11>0</M11>
                <M12>0</M12>
                <M13>1</M13>
                <M14>0</M14>
                <M21>0</M21>
                <M22>1</M22>
                <M23>0</M23>
                <M24>0</M24>
                <M31>-1</M31>
                <M32>0</M32>
                <M33>0</M33>
                <M34>0</M34>
                <M41>-29.2</M41>
                <M42>0</M42>
                <M43>-60</M43>
                <M44>1</M44>
            </transform>
        </Entity>
    </entities>
</EntityList>)";

    XmlSerializer<EntityListData> serializer;
    EntityListData list = serializer.Deserialize(fixture);

    ASSERT_EQ(list.entities.size(), 1u);
    EXPECT_EQ(list.entities[0].name, "spawn0");
    // The exact texels of the transform, not merely "it parsed": a reader that mixed up row and
    // column order would still populate sixteen floats.
    EXPECT_FLOAT_EQ(list.entities[0].transform.M13, 1.0f);
    EXPECT_FLOAT_EQ(list.entities[0].transform.M31, -1.0f);
    EXPECT_FLOAT_EQ(list.entities[0].transform.M41, -29.2f);
    EXPECT_FLOAT_EQ(list.entities[0].transform.M43, -60.0f);
    EXPECT_FLOAT_EQ(list.entities[0].transform.M44, 1.0f);

    EntityListData again = serializer.Deserialize(serializer.Serialize(list));
    ASSERT_EQ(again.entities.size(), 1u);
    EXPECT_TRUE(again.entities[0] == list.entities[0]);
}

// Transcribed from ShipGame/Content/levels/level1/level1_lights.xml.
TEST(XnaFixtureTests, ShipGameLightList_ExcerptParsesAndRoundTrips) {
    const std::string fixture = R"(<?xml version="1.0"?>
<LightList xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema">
  <ambient>
    <X>0.2</X>
    <Y>0.2</Y>
    <Z>0.2</Z>
  </ambient>
  <lights>
    <Light>
        <position>
            <X>0</X>
            <Y>500</Y>
            <Z>0</Z>
        </position>
        <radius>1000</radius>
        <color>
            <X>0.7</X>
            <Y>0.7</Y>
            <Z>0.7</Z>
        </color>
    </Light>
  </lights>
</LightList>)";

    XmlSerializer<LightListData> serializer;
    LightListData lights = serializer.Deserialize(fixture);

    EXPECT_FLOAT_EQ(lights.ambient.X, 0.2f);
    ASSERT_EQ(lights.lights.size(), 1u);
    EXPECT_FLOAT_EQ(lights.lights[0].position.Y, 500.0f);
    EXPECT_FLOAT_EQ(lights.lights[0].radius, 1000.0f);
    EXPECT_FLOAT_EQ(lights.lights[0].color.Z, 0.7f);

    LightListData again = serializer.Deserialize(serializer.Serialize(lights));
    EXPECT_TRUE(again.ambient == lights.ambient);
    ASSERT_EQ(again.lights.size(), 1u);
    EXPECT_TRUE(again.lights[0] == lights.lights[0]);
}

// Transcribed from Spacewar_4_0/settings.xml (two-space indentation, as shipped).
TEST(XnaFixtureTests, SpacewarSettings_ExcerptParsesAndRoundTrips) {
    const std::string fixture = R"(<?xml version="1.0"?>
<Settings xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema">
  <MediaPath>content\</MediaPath>
  <WindowTitle>Spacewar</WindowTitle>
  <LevelTime>30</LevelTime>
  <ThrustPower>100</ThrustPower>
  <FrictionFactor>0.1</FrictionFactor>
  <MaxSpeed>200</MaxSpeed>
  <ShipRecoveryTime>1.6</ShipRecoveryTime>
  <SunPosition>
    <X>0</X>
    <Y>0</Y>
  </SunPosition>
  <Ships>
    <PlayerShipInfo>
      <StartPosition>
        <X>-300</X>
        <Y>0</Y>
      </StartPosition>
      <StartAngle>90</StartAngle>
    </PlayerShipInfo>
    <PlayerShipInfo>
      <StartPosition>
        <X>300</X>
        <Y>0</Y>
      </StartPosition>
      <StartAngle>90</StartAngle>
    </PlayerShipInfo>
  </Ships>
  <Weapons>
    <WeaponInfo>
      <Cost>0</Cost>
      <Lifetime>3</Lifetime>
      <Max>5</Max>
      <Burst>1</Burst>
      <Acceleration>0</Acceleration>
      <Damage>1</Damage>
    </WeaponInfo>
  </Weapons>
</Settings>)";

    XmlSerializer<SettingsData> serializer;
    SettingsData settings = serializer.Deserialize(fixture);

    // A backslash in text content survives unescaped, and the interleaved unregistered members
    // (ThrustPower, MaxSpeed) are skipped without disturbing the ones that follow them.
    EXPECT_EQ(settings.MediaPath, "content\\");
    EXPECT_EQ(settings.WindowTitle, "Spacewar");
    EXPECT_EQ(settings.LevelTime, 30);
    EXPECT_FLOAT_EQ(settings.FrictionFactor, 0.1f);
    EXPECT_FLOAT_EQ(settings.ShipRecoveryTime, 1.6f);

    ASSERT_EQ(settings.Ships.size(), 2u);
    EXPECT_FLOAT_EQ(settings.Ships[0].StartPosition.X, -300.0f);
    EXPECT_FLOAT_EQ(settings.Ships[1].StartPosition.X, 300.0f);
    EXPECT_FLOAT_EQ(settings.Ships[1].StartAngle, 90.0f);

    ASSERT_EQ(settings.Weapons.size(), 1u);
    EXPECT_EQ(settings.Weapons[0].Max, 5);
    EXPECT_FLOAT_EQ(settings.Weapons[0].Lifetime, 3.0f);

    SettingsData again = serializer.Deserialize(serializer.Serialize(settings));
    EXPECT_EQ(again.MediaPath, settings.MediaPath);
    ASSERT_EQ(again.Ships.size(), 2u);
    EXPECT_TRUE(again.Ships[0] == settings.Ships[0]);
    ASSERT_EQ(again.Weapons.size(), 1u);
    EXPECT_TRUE(again.Weapons[0] == settings.Weapons[0]);
}

// The float that made the exponent-case deviation visible, from NetRumble's rocketTrail.xml.
// .NET Framework wrote nine significant digits; the shortest round-trippable form has eight.
// Both must parse to the identical float, and what this module emits must use the uppercase
// `E` the XML Schema canonical form (and every authentic fixture) uses.
TEST(XnaFixtureTests, NetRumbleFloatMax_ParsesFromDotNetFormAndReEmitsUppercaseExponent) {
    XmlSerializer<DurationHolder> serializer;
    DurationHolder fromDotNet =
        serializer.Deserialize("<ParticleSystem><Duration>3.40282347E+38</Duration></ParticleSystem>");

    EXPECT_EQ(fromDotNet.Duration, std::numeric_limits<float>::max());

    const std::string emitted = serializer.Serialize(fromDotNet);
    EXPECT_NE(emitted.find("E+38"), std::string::npos) << emitted;
    EXPECT_EQ(emitted.find("e+38"), std::string::npos) << emitted;

    // And what we emit parses back to the same value .NET's spelling did.
    EXPECT_EQ(serializer.Deserialize(emitted).Duration, fromDotNet.Duration);
}

// ===============================================================================================
// 2. The real files, when the XNA source tree is available.
// ===============================================================================================

TEST(XnaRealFixtureTests, ShipGameLevel1Spawns_LoadsEveryEntity) {
    const std::string path =
        SamplesRoot() + "/ShipGame_4_0/ShipGame/Content/levels/level1/level1_spawns.xml";
    const std::string fixture = ReadFileOrEmpty(path);
    if (fixture.empty()) GTEST_SKIP() << "XNA sample tree not present at " << path;

    XmlSerializer<EntityListData> serializer;
    EntityListData list = serializer.Deserialize(WithoutBom(fixture));

    // The exact count the shipped file contains. ">= 2" would pass on a parse that produced
    // nothing, which a planted item-renaming defect demonstrated -- see the sweep test below.
    ASSERT_EQ(list.entities.size(), 2u);
    for (const EntityData& entity : list.entities) {
        EXPECT_FALSE(entity.name.empty());
    }

    // Round trip through our own writer and back: same count, same names, same transforms.
    EntityListData again = serializer.Deserialize(serializer.Serialize(list));
    ASSERT_EQ(again.entities.size(), list.entities.size());
    for (std::size_t i = 0; i < list.entities.size(); ++i) {
        EXPECT_TRUE(again.entities[i] == list.entities[i]) << "entity " << i;
    }
}

TEST(XnaRealFixtureTests, ShipGameLevel1Lights_LoadsEveryLight) {
    const std::string path =
        SamplesRoot() + "/ShipGame_4_0/ShipGame/Content/levels/level1/level1_lights.xml";
    const std::string fixture = ReadFileOrEmpty(path);
    if (fixture.empty()) GTEST_SKIP() << "XNA sample tree not present at " << path;

    XmlSerializer<LightListData> serializer;
    LightListData lights = serializer.Deserialize(WithoutBom(fixture));

    ASSERT_EQ(lights.lights.size(), 1u);
    // A light with a zero radius would be invisible in the game; none of the shipped ones are,
    // so this is a real content assertion rather than a shape one.
    for (const LightData& light : lights.lights) {
        EXPECT_GT(light.radius, 0.0f);
    }

    LightListData again = serializer.Deserialize(serializer.Serialize(lights));
    ASSERT_EQ(again.lights.size(), lights.lights.size());
    for (std::size_t i = 0; i < lights.lights.size(); ++i) {
        EXPECT_TRUE(again.lights[i] == lights.lights[i]) << "light " << i;
    }
}

TEST(XnaRealFixtureTests, ShipGameShip1_LoadsEveryHardpoint) {
    const std::string path = SamplesRoot() + "/ShipGame_4_0/ShipGame/Content/ships/ship1.xml";
    const std::string fixture = ReadFileOrEmpty(path);
    if (fixture.empty()) GTEST_SKIP() << "XNA sample tree not present at " << path;

    XmlSerializer<EntityListData> serializer;
    EntityListData list = serializer.Deserialize(WithoutBom(fixture));

    ASSERT_EQ(list.entities.size(), 4u) << "ship1 ships four hardpoints";
    EntityListData again = serializer.Deserialize(serializer.Serialize(list));
    ASSERT_EQ(again.entities.size(), list.entities.size());
    for (std::size_t i = 0; i < list.entities.size(); ++i) {
        EXPECT_TRUE(again.entities[i] == list.entities[i]) << "entity " << i;
    }
}

TEST(XnaRealFixtureTests, SpacewarSettings_LoadsShipsAndWeapons) {
    const std::string path = SamplesRoot() + "/Spacewar_4_0/settings.xml";
    const std::string fixture = ReadFileOrEmpty(path);
    if (fixture.empty()) GTEST_SKIP() << "XNA sample tree not present at " << path;

    XmlSerializer<SettingsData> serializer;
    SettingsData settings = serializer.Deserialize(WithoutBom(fixture));

    EXPECT_EQ(settings.WindowTitle, "Spacewar");
    EXPECT_FALSE(settings.MediaPath.empty());
    ASSERT_EQ(settings.Ships.size(), 2u) << "Spacewar is a two-player game";
    ASSERT_EQ(settings.Weapons.size(), 5u) << "settings.xml ships five weapon definitions";

    // ShipLighting[] -- a C# array field, five Vector4s deep, with the leading-dot float
    // spelling (<X>.4</X>) the file really uses.
    ASSERT_EQ(settings.ShipLights.size(), 2u);
    EXPECT_FLOAT_EQ(settings.ShipLights[0].Ambient.X, 0.4f);
    EXPECT_FLOAT_EQ(settings.ShipLights[0].Ambient.W, 1.0f);
    EXPECT_FLOAT_EQ(settings.ShipLights[0].DirectionalColor.X, 0.639f);
    EXPECT_FLOAT_EQ(settings.ShipLights[0].DirectionalColor.Z, 0.937f);
    EXPECT_FLOAT_EQ(settings.ShipLights[0].PointFactor, 0.0001f);
    EXPECT_FLOAT_EQ(settings.ShipLights[1].Ambient.X, 0.3f);

    // Keys enumerators, by name, from Microsoft's own file. Three distinct values, so a reader
    // that returned a constant fails.
    EXPECT_EQ(settings.Player1Start, KeysData::LeftControl);
    EXPECT_EQ(settings.Player2Start, KeysData::RightControl);
    EXPECT_EQ(settings.Player2RightTrigger, KeysData::Delete);

    SettingsData again = serializer.Deserialize(serializer.Serialize(settings));
    EXPECT_EQ(again.WindowTitle, settings.WindowTitle);
    ASSERT_EQ(again.Ships.size(), settings.Ships.size());
    for (std::size_t i = 0; i < settings.Ships.size(); ++i) {
        EXPECT_TRUE(again.Ships[i] == settings.Ships[i]) << "ship " << i;
    }
    ASSERT_EQ(again.Weapons.size(), settings.Weapons.size());
    for (std::size_t i = 0; i < settings.Weapons.size(); ++i) {
        EXPECT_TRUE(again.Weapons[i] == settings.Weapons[i]) << "weapon " << i;
    }
    ASSERT_EQ(again.ShipLights.size(), settings.ShipLights.size());
    for (std::size_t i = 0; i < settings.ShipLights.size(); ++i) {
        EXPECT_TRUE(again.ShipLights[i] == settings.ShipLights[i]) << "ship lighting " << i;
    }
    EXPECT_EQ(again.Player1Start, settings.Player1Start);
    EXPECT_EQ(again.Player2Start, settings.Player2Start);
    EXPECT_EQ(again.Player2RightTrigger, settings.Player2RightTrigger);
}

// Every EntityList-shaped file the ShipGame content tree ships, in one sweep: a per-file failure
// names the file, so a single divergent one is identifiable rather than hidden behind the first.
//
// **The expected entity count per file is spelled out, and that is not decoration.** An earlier
// version of this test asserted only that the round trip preserved the count, and it PASSED
// under a planted defect that renamed every list item: the fixture parsed to zero entities, the
// re-serialization wrote zero, the re-parse read zero, the counts matched and the comparison
// loop never ran. Measured, not hypothesised. A test that cannot distinguish "loaded correctly"
// from "loaded nothing" is not a test, so the counts below (verified against the shipped files)
// are what make a silent-empty-parse regression fail here.
TEST(XnaRealFixtureTests, EveryShipGameEntityListFixtureRoundTrips) {
    const std::vector<std::pair<std::string, std::size_t>> fixtures = {
        {"/ShipGame_4_0/ShipGame/Content/levels/level1/level1_spawns.xml", 2},
        {"/ShipGame_4_0/ShipGame/Content/levels/level1/level1_powerups.xml", 4},
        {"/ShipGame_4_0/ShipGame/Content/levels/level2/level2_spawns.xml", 2},
        {"/ShipGame_4_0/ShipGame/Content/levels/level2/level2_powerups.xml", 4},
        {"/ShipGame_4_0/ShipGame/Content/ships/ship1.xml", 4},
        {"/ShipGame_4_0/ShipGame/Content/ships/ship2.xml", 4},
    };

    XmlSerializer<EntityListData> serializer;
    std::size_t checked = 0;
    for (const auto& [relative, expectedEntities] : fixtures) {
        const std::string fixture = ReadFileOrEmpty(SamplesRoot() + relative);
        if (fixture.empty()) continue;

        EntityListData list = serializer.Deserialize(WithoutBom(fixture));
        ASSERT_EQ(list.entities.size(), expectedEntities) << relative;
        for (const EntityData& entity : list.entities) {
            EXPECT_FALSE(entity.name.empty()) << relative;
        }

        EntityListData again = serializer.Deserialize(serializer.Serialize(list));
        ASSERT_EQ(again.entities.size(), expectedEntities) << relative;
        for (std::size_t i = 0; i < list.entities.size(); ++i) {
            EXPECT_TRUE(again.entities[i] == list.entities[i]) << relative << " entity " << i;
        }
        ++checked;
    }

    if (checked == 0) GTEST_SKIP() << "XNA sample tree not present at " << SamplesRoot();
    EXPECT_EQ(checked, fixtures.size()) << "some shipped fixtures were missing from the tree";
}
