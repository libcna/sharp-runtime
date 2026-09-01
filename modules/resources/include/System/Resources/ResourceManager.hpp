// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "System/Globalization/CultureInfo.hpp"

namespace System::Resources {

    /**
     * @brief Retrieves culture-specific string resources and applies the .NET parent-culture
     * fallback order.
     *
     * C++ counterpart of .NET System.Resources.ResourceManager's string lookup surface.
     * Sharp Runtime intentionally has no reflection or Assembly metadata, so resource producers
     * supply an AOT lookup callback. The callback performs one exact-culture lookup; this class
     * owns the observable full-culture, parent-culture, and invariant fallback policy.
     */
    class ResourceManager final {
    public:
        /**
         * @brief Exact-resource lookup callback used by generated or hand-authored AOT resource
         * tables.
         *
         * @param baseName Logical resource base name.
         * @param cultureName Exact culture to inspect, or an empty string for invariant resources.
         * @param resourceName Case-sensitive resource key.
         * @return The exact resource value, including an empty string, or std::nullopt when absent.
         */
        using ResourceLookup = std::function<std::optional<std::string>(
            std::string_view baseName,
            std::string_view cultureName,
            std::string_view resourceName)>;

        /**
         * @brief Constructs a manager backed by an AOT resource lookup callback.
         *
         * This constructor is the compile-time C++ adaptation of .NET's Assembly-backed
         * constructor. It does not introduce runtime reflection.
         * @param baseName Root name of the resource family.
         * @param lookup Callback that performs one exact-culture lookup.
         * @throws System::ArgumentNullException if @p lookup is empty.
         */
        ResourceManager(std::string baseName, ResourceLookup lookup);

        /**
         * @brief Gets the root name of the resource family.
         *
         * C++ counterpart of .NET ResourceManager.BaseName.
         * @return The base name supplied to the constructor.
         */
        [[nodiscard]] const std::string& getBaseNameProperty() const;

        /**
         * @brief Gets a string using the current thread's UI culture.
         *
         * C++ counterpart of .NET ResourceManager.GetString(string).
         * @param name Case-sensitive resource key.
         * @return The resolved value, or std::nullopt when no culture in the fallback chain
         * contains @p name.
         */
        [[nodiscard]] std::optional<std::string> GetString(const std::string& name) const;

        /**
         * @brief Gets a string using an explicitly selected culture.
         *
         * C++ counterpart of .NET ResourceManager.GetString(string, CultureInfo).
         * @param name Case-sensitive resource key.
         * @param culture Culture at which to begin lookup.
         * @return The resolved value, or std::nullopt when absent throughout the fallback chain.
         */
        [[nodiscard]] std::optional<std::string> GetString(
            const std::string& name,
            const System::Globalization::CultureInfo& culture) const;

        /**
         * @brief Gets a string using an optional culture override.
         *
         * A missing override has the same meaning as a null CultureInfo in .NET: use the current
         * UI culture.
         * @param name Case-sensitive resource key.
         * @param culture Culture override, or std::nullopt for CurrentUICulture.
         * @return The resolved value, or std::nullopt when absent throughout the fallback chain.
         */
        [[nodiscard]] std::optional<std::string> GetString(
            const std::string& name,
            const std::optional<System::Globalization::CultureInfo>& culture) const;

    private:
        std::string baseName_;
        ResourceLookup lookup_;
    };

} // namespace System::Resources
