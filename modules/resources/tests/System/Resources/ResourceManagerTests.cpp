// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

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
