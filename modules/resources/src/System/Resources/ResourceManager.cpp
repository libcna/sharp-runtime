// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include "System/Resources/ResourceManager.hpp"

#include <utility>

#include "System/ArgumentNullException.hpp"

namespace System::Resources {

    ResourceManager::ResourceManager(std::string baseName, ResourceLookup lookup)
        : baseName_(std::move(baseName)), lookup_(std::move(lookup)) {
        if (!lookup_) {
            throw System::ArgumentNullException("lookup");
        }
    }

    const std::string& ResourceManager::getBaseNameProperty() const {
        return baseName_;
    }

    std::optional<std::string> ResourceManager::GetString(const std::string& name) const {
        return GetString(name, System::Globalization::CultureInfo::getCurrentUICultureProperty());
    }

    std::optional<std::string> ResourceManager::GetString(
        const std::string& name,
        const System::Globalization::CultureInfo& culture) const {
        std::string cultureName = culture.getNameProperty();
        for (;;) {
            if (std::optional<std::string> value = lookup_(baseName_, cultureName, name)) {
                return value;
            }

            if (cultureName.empty()) {
                return std::nullopt;
            }

            const std::size_t separator = cultureName.rfind('-');
            cultureName = separator == std::string::npos
                ? std::string{}
                : cultureName.substr(0, separator);
        }
    }

    std::optional<std::string> ResourceManager::GetString(
        const std::string& name,
        const std::optional<System::Globalization::CultureInfo>& culture) const {
        return culture.has_value() ? GetString(name, *culture) : GetString(name);
    }

} // namespace System::Resources
