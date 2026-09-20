// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors

#include <any>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/Globalization/CultureInfo.hpp"
#include "System/Resources/ResourceManager.hpp"

namespace {

    struct Entry {
        std::string_view baseName;
        std::string_view cultureName;
        std::string_view resourceName;
        std::string_view value;
    };

    constexpr std::array<Entry, 7> Entries{{
        {"Example.Strings", "", "Greeting", "neutral greeting"},
        {"Example.Strings", "", "Empty", ""},
        {"Example.Strings", "fr", "Greeting", "bonjour"},
        {"Example.Strings", "zh-Hant", "Greeting", "traditional greeting"},
        {"Example.Strings", "zh-Hant-TW", "Greeting", "Taiwan greeting"},
        {"Example.Strings", "en-US", "Greeting", "hello"},
        {"Other.Strings", "en-US", "Greeting", "other hello"},
    }};

    std::optional<std::string> Lookup(
        const std::string_view baseName,
        const std::string_view cultureName,
        const std::string_view resourceName) {
        for (const Entry& entry : Entries) {
            if (entry.baseName == baseName && entry.cultureName == cultureName &&
                entry.resourceName == resourceName) {
                return std::string(entry.value);
            }
        }
        return std::nullopt;
    }

} // namespace

TEST(ResourceManagerTest, ConstructorRequiresLookupCallback) {
    EXPECT_THROW(
        (void)System::Resources::ResourceManager("Example.Strings", {}),
        System::ArgumentNullException);
}

TEST(ResourceManagerTest, BaseNameReturnsConstructorValue) {
    const System::Resources::ResourceManager manager("Example.Strings", Lookup);
    EXPECT_EQ(manager.getBaseNameProperty(), "Example.Strings");
}

TEST(ResourceManagerTest, ExplicitCultureUsesExactResourceFirst) {
    const System::Resources::ResourceManager manager("Example.Strings", Lookup);
    EXPECT_EQ(
        manager.GetString("Greeting", System::Globalization::CultureInfo("zh-Hant-TW")),
        "Taiwan greeting");
}

TEST(ResourceManagerTest, ExplicitCultureFallsBackThroughEveryParent) {
    const System::Resources::ResourceManager manager("Example.Strings", Lookup);
    EXPECT_EQ(
        manager.GetString("Greeting", System::Globalization::CultureInfo("zh-Hant-HK")),
        "traditional greeting");
    EXPECT_EQ(
        manager.GetString("Greeting", System::Globalization::CultureInfo("fr-CA")),
        "bonjour");
}

TEST(ResourceManagerTest, ExplicitCultureFallsBackToInvariantResources) {
    const System::Resources::ResourceManager manager("Example.Strings", Lookup);
    EXPECT_EQ(
        manager.GetString("Greeting", System::Globalization::CultureInfo("da-DK")),
        "neutral greeting");
}

TEST(ResourceManagerTest, MissingOptionalCultureUsesCurrentUICulture) {
    const System::Globalization::CultureInfo previous =
        System::Globalization::CultureInfo::getCurrentUICultureProperty();
    System::Globalization::CultureInfo::setCurrentUICultureProperty(
        System::Globalization::CultureInfo("en-US"));

    const System::Resources::ResourceManager manager("Example.Strings", Lookup);
    const std::optional<System::Globalization::CultureInfo> noOverride;
    EXPECT_EQ(manager.GetString("Greeting", noOverride), "hello");
    EXPECT_EQ(manager.GetString("Greeting"), "hello");

    System::Globalization::CultureInfo::setCurrentUICultureProperty(previous);
}

TEST(ResourceManagerTest, LookupIsCaseSensitiveAndPreservesEmptyValues) {
    const System::Resources::ResourceManager manager("Example.Strings", Lookup);
    const System::Globalization::CultureInfo culture("en-US");
    EXPECT_FALSE(manager.GetString("greeting", culture).has_value());
    ASSERT_TRUE(manager.GetString("Empty", culture).has_value());
    EXPECT_TRUE(manager.GetString("Empty", culture)->empty());
}

TEST(ResourceManagerTest, BaseNameSeparatesResourceFamilies) {
    const System::Resources::ResourceManager manager("Other.Strings", Lookup);
    EXPECT_EQ(
        manager.GetString("Greeting", System::Globalization::CultureInfo("en-US")),
        "other hello");
}

// ---------------------------------------------------------------------------
// GetObject / GetByteArray: the non-string half of the lookup surface.
//
// .NET's GetString is GetObject plus a cast, so a string resource is an object resource here too,
// and both kinds resolve within a culture before the fallback moves outwards.
// ---------------------------------------------------------------------------

namespace {

    struct BinaryEntry {
        std::string_view baseName;
        std::string_view cultureName;
        std::string_view resourceName;
        std::vector<std::uint8_t> value;
    };

    const std::vector<BinaryEntry>& BinaryEntries() {
        static const std::vector<BinaryEntry> entries{
            {"Example.Strings", "", "Blob", {1, 2, 3}},
            {"Example.Strings", "fr", "Blob", {9, 9}},
            {"Example.Strings", "", "Count", {}},
        };
        return entries;
    }

    std::optional<std::any> ObjectLookup(
        const std::string_view baseName,
        const std::string_view cultureName,
        const std::string_view resourceName) {
        for (const BinaryEntry& entry : BinaryEntries()) {
            if (entry.baseName == baseName && entry.cultureName == cultureName &&
                entry.resourceName == resourceName) {
                return std::any(entry.value);
            }
        }
        // A non-binary, non-string object resource, to show GetObject is not byte-array-only.
        if (baseName == "Example.Strings" && cultureName.empty() && resourceName == "Number") {
            return std::any(static_cast<SharpRuntime::intcs>(42));
        }
        return std::nullopt;
    }

} // namespace

TEST(ResourceManagerTest, ObjectConstructorRequiresBothCallbacks) {
    EXPECT_THROW(
        (void)System::Resources::ResourceManager("Example.Strings", {}, ObjectLookup),
        System::ArgumentNullException);
    EXPECT_THROW(
        (void)System::Resources::ResourceManager("Example.Strings", Lookup,
                                                System::Resources::ResourceManager::ObjectLookup{}),
        System::ArgumentNullException);
}

TEST(ResourceManagerTest, GetObjectResolvesABinaryResource) {
    const System::Resources::ResourceManager manager("Example.Strings", Lookup, ObjectLookup);
    const auto value = manager.GetObject("Blob", System::Globalization::CultureInfo(""));
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(std::any_cast<std::vector<std::uint8_t>>(*value),
              (std::vector<std::uint8_t>{1, 2, 3}));
}

TEST(ResourceManagerTest, GetObjectResolvesANonBinaryNonStringResource) {
    const System::Resources::ResourceManager manager("Example.Strings", Lookup, ObjectLookup);
    const auto value = manager.GetObject("Number", System::Globalization::CultureInfo(""));
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(std::any_cast<SharpRuntime::intcs>(*value), 42);
}

TEST(ResourceManagerTest, GetObjectReturnsAStringResourceAsAString) {
    // With no object lookup at all: a string resource is an object resource, as in .NET.
    const System::Resources::ResourceManager stringOnly("Example.Strings", Lookup);
    const auto value = stringOnly.GetObject("Greeting", System::Globalization::CultureInfo("fr"));
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(std::any_cast<std::string>(*value), "bonjour");
}

TEST(ResourceManagerTest, GetObjectAppliesTheSameCultureFallbackAsGetString) {
    const System::Resources::ResourceManager manager("Example.Strings", Lookup, ObjectLookup);

    // zh-Hant-HK has no Blob of its own, nor does zh-Hant, so the invariant one answers.
    const auto fallenBack =
        manager.GetObject("Blob", System::Globalization::CultureInfo("zh-Hant-HK"));
    ASSERT_TRUE(fallenBack.has_value());
    EXPECT_EQ(std::any_cast<std::vector<std::uint8_t>>(*fallenBack),
              (std::vector<std::uint8_t>{1, 2, 3}));

    // fr has its own, which must win over the invariant one.
    const auto exact = manager.GetObject("Blob", System::Globalization::CultureInfo("fr"));
    ASSERT_TRUE(exact.has_value());
    EXPECT_EQ(std::any_cast<std::vector<std::uint8_t>>(*exact), (std::vector<std::uint8_t>{9, 9}));
}

TEST(ResourceManagerTest, ACultureSpecificObjectWinsOverAnInvariantString) {
    // Both lookups are asked at each culture before the fallback moves outwards, so an invariant
    // string cannot shadow a culture-specific object of the same name.
    const auto stringLookup = [](std::string_view baseName, std::string_view cultureName,
                                 std::string_view resourceName) -> std::optional<std::string> {
        if (baseName == "Shadow" && cultureName.empty() && resourceName == "Item") {
            return std::string("invariant string");
        }
        return std::nullopt;
    };
    const auto objectLookup = [](std::string_view baseName, std::string_view cultureName,
                                 std::string_view resourceName) -> std::optional<std::any> {
        if (baseName == "Shadow" && cultureName == "fr" && resourceName == "Item") {
            return std::any(std::vector<std::uint8_t>{7});
        }
        return std::nullopt;
    };

    const System::Resources::ResourceManager manager("Shadow", stringLookup, objectLookup);
    const auto french = manager.GetObject("Item", System::Globalization::CultureInfo("fr"));
    ASSERT_TRUE(french.has_value());
    EXPECT_EQ(std::any_cast<std::vector<std::uint8_t>>(*french), (std::vector<std::uint8_t>{7}));

    // And the invariant string is still what an invariant lookup finds.
    const auto invariant = manager.GetObject("Item", System::Globalization::CultureInfo(""));
    ASSERT_TRUE(invariant.has_value());
    EXPECT_EQ(std::any_cast<std::string>(*invariant), "invariant string");
}

TEST(ResourceManagerTest, GetObjectReturnsNulloptForAnAbsentName) {
    const System::Resources::ResourceManager manager("Example.Strings", Lookup, ObjectLookup);
    EXPECT_FALSE(manager.GetObject("Absent", System::Globalization::CultureInfo("")).has_value());
}

TEST(ResourceManagerTest, GetByteArrayAnswersOnlyForBinaryResources) {
    const System::Resources::ResourceManager manager("Example.Strings", Lookup, ObjectLookup);

    const auto bytes = manager.GetByteArray("Blob", System::Globalization::CultureInfo(""));
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, (std::vector<std::uint8_t>{1, 2, 3}));

    // An empty binary resource is a present one, not an absent one.
    const auto empty = manager.GetByteArray("Count", System::Globalization::CultureInfo(""));
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(empty->empty());

    // A string and an int are both present but neither is binary.
    EXPECT_FALSE(manager.GetByteArray("Greeting", System::Globalization::CultureInfo("")).has_value());
    EXPECT_FALSE(manager.GetByteArray("Number", System::Globalization::CultureInfo("")).has_value());
    EXPECT_FALSE(manager.GetByteArray("Absent", System::Globalization::CultureInfo("")).has_value());
}
