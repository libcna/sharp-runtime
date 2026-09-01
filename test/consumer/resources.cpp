// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors

#include <optional>
#include <string>
#include <string_view>

#include "System/Globalization/CultureInfo.hpp"
#include "System/Resources/ResourceManager.hpp"

namespace {

    std::optional<std::string> Lookup(
        const std::string_view baseName,
        const std::string_view cultureName,
        const std::string_view resourceName) {
        if (baseName == "Consumer.Strings" && cultureName == "fr" &&
            resourceName == "Greeting") {
            return "bonjour";
        }
        return std::nullopt;
    }

} // namespace

int main() {
    const System::Resources::ResourceManager manager("Consumer.Strings", Lookup);
    return manager.GetString(
               "Greeting",
               System::Globalization::CultureInfo("fr-CA")) == "bonjour"
        ? 0
        : 1;
}
