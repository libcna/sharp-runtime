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

    ResourceManager::ResourceManager(std::string baseName, ResourceLookup lookup,
                                     ObjectLookup objectLookup)
        : baseName_(std::move(baseName)),
          lookup_(std::move(lookup)),
          objectLookup_(std::move(objectLookup)) {
        if (!lookup_) {
            throw System::ArgumentNullException("lookup");
        }
        if (!objectLookup_) {
            throw System::ArgumentNullException("objectLookup");
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
        const std::function<std::optional<std::string>(const std::string&)> probe =
            [this, &name](const std::string& cultureName) {
                return lookup_(baseName_, cultureName, name);
            };
        return WithCultureFallback<std::string>(culture.getNameProperty(), probe);
    }

    std::optional<std::string> ResourceManager::GetString(
        const std::string& name,
        const std::optional<System::Globalization::CultureInfo>& culture) const {
        return culture.has_value() ? GetString(name, *culture) : GetString(name);
    }

    std::optional<std::any> ResourceManager::GetObject(const std::string& name) const {
        return GetObject(name, System::Globalization::CultureInfo::getCurrentUICultureProperty());
    }

    std::optional<std::any> ResourceManager::GetObject(
        const std::string& name,
        const System::Globalization::CultureInfo& culture) const {
        // One probe per culture, asking the object lookup first and the string lookup second, so a
        // family holding both kinds resolves a non-string resource without the string lookup having
        // to pretend it can. Both are asked at the SAME culture before falling back to the parent:
        // .NET resolves a resource within a culture before moving outwards, and asking one lookup
        // through the whole chain before the other would let an invariant string shadow a
        // culture-specific object.
        const std::function<std::optional<std::any>(const std::string&)> probe =
            [this, &name](const std::string& cultureName) -> std::optional<std::any> {
                if (objectLookup_) {
                    if (std::optional<std::any> value = objectLookup_(baseName_, cultureName, name)) {
                        return value;
                    }
                }
                if (std::optional<std::string> text = lookup_(baseName_, cultureName, name)) {
                    return std::any(*std::move(text));
                }
                return std::nullopt;
            };
        return WithCultureFallback<std::any>(culture.getNameProperty(), probe);
    }

    std::optional<std::any> ResourceManager::GetObject(
        const std::string& name,
        const std::optional<System::Globalization::CultureInfo>& culture) const {
        return culture.has_value() ? GetObject(name, *culture) : GetObject(name);
    }

    std::optional<std::vector<std::uint8_t>> ResourceManager::GetByteArray(
        const std::string& name) const {
        return GetByteArray(name, System::Globalization::CultureInfo::getCurrentUICultureProperty());
    }

    std::optional<std::vector<std::uint8_t>> ResourceManager::GetByteArray(
        const std::string& name,
        const System::Globalization::CultureInfo& culture) const {
        const std::optional<std::any> value = GetObject(name, culture);
        if (!value.has_value()) {
            return std::nullopt;
        }
        const auto* bytes = std::any_cast<std::vector<std::uint8_t>>(&*value);
        return bytes == nullptr ? std::nullopt : std::optional<std::vector<std::uint8_t>>(*bytes);
    }

} // namespace System::Resources
