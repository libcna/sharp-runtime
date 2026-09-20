// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <any>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "System/Globalization/CultureInfo.hpp"

namespace System::Resources {

    /**
     * @brief Retrieves culture-specific string resources and applies the .NET parent-culture
     * fallback order.
     *
     * C++ counterpart of .NET System.Resources.ResourceManager's string and object lookup surface.
     * Sharp Runtime intentionally has no reflection or Assembly metadata, so resource producers
     * supply an AOT lookup callback. The callback performs one exact-culture lookup; this class
     * owns the observable full-culture, parent-culture, and invariant fallback policy.
     *
     * A resource family may be string-only, which is the common case and what the two-argument
     * constructor serves, or may also hold non-string resources -- a binary blob, most usefully --
     * which the three-argument constructor's object lookup serves. GetObject() answers for both: a
     * string resource *is* an object resource, exactly as in .NET, where GetString is GetObject plus
     * a cast.
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
         * @brief Exact-resource lookup callback for resources that are not strings.
         *
         * The object counterpart of ResourceLookup, for a resource family that holds non-string
         * values. A binary resource is a `std::vector<std::uint8_t>`, which is what a caller
         * expecting .NET's `byte[]` asks GetObject for.
         *
         * @param baseName Logical resource base name.
         * @param cultureName Exact culture to inspect, or an empty string for invariant resources.
         * @param resourceName Case-sensitive resource key.
         * @return The exact resource value, or std::nullopt when absent.
         */
        using ObjectLookup = std::function<std::optional<std::any>(
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
         * @brief Constructs a manager whose family also holds resources that are not strings.
         *
         * @param baseName Root name of the resource family.
         * @param lookup Callback that performs one exact-culture string lookup.
         * @param objectLookup Callback that performs one exact-culture lookup of any resource,
         *        consulted by GetObject() before the string lookup.
         * @throws System::ArgumentNullException if @p lookup or @p objectLookup is empty.
         */
        ResourceManager(std::string baseName, ResourceLookup lookup, ObjectLookup objectLookup);

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

        /**
         * @brief Gets a resource of any type using the current thread's UI culture.
         *
         * C++ counterpart of .NET ResourceManager.GetObject(string). Applies the same culture
         * fallback GetString() applies.
         *
         * @param name Case-sensitive resource key.
         * @return The resolved value, or std::nullopt when no culture in the fallback chain contains
         *         @p name. A string resource comes back as a `std::string`, whether it was found
         *         through the object lookup or the string one.
         */
        [[nodiscard]] std::optional<std::any> GetObject(const std::string& name) const;

        /**
         * @brief Gets a resource of any type using an explicitly selected culture.
         *
         * C++ counterpart of .NET ResourceManager.GetObject(string, CultureInfo).
         *
         * @param name Case-sensitive resource key.
         * @param culture Culture at which to begin lookup.
         * @return The resolved value, or std::nullopt when absent throughout the fallback chain.
         */
        [[nodiscard]] std::optional<std::any> GetObject(
            const std::string& name,
            const System::Globalization::CultureInfo& culture) const;

        /**
         * @brief Gets a resource of any type using an optional culture override.
         *
         * A missing override has the same meaning as a null CultureInfo in .NET: use the current
         * UI culture.
         *
         * @param name Case-sensitive resource key.
         * @param culture Culture override, or std::nullopt for CurrentUICulture.
         * @return The resolved value, or std::nullopt when absent throughout the fallback chain.
         */
        [[nodiscard]] std::optional<std::any> GetObject(
            const std::string& name,
            const std::optional<System::Globalization::CultureInfo>& culture) const;

        /**
         * @brief Gets a binary resource using the current thread's UI culture.
         *
         * The `byte[]` case of GetObject(), which is what .NET resource families carry binary
         * content as and the one non-string shape a consumer reaches for by name. Not a .NET member;
         * .NET's caller casts GetObject's result, which in C++ means naming the stored type, and a
         * named accessor is clearer than making every caller spell the cast.
         *
         * @param name Case-sensitive resource key.
         * @return The bytes, or std::nullopt when @p name is absent or is not a binary resource.
         */
        [[nodiscard]] std::optional<std::vector<std::uint8_t>> GetByteArray(
            const std::string& name) const;

        /**
         * @brief Gets a binary resource using an explicitly selected culture.
         *
         * @param name Case-sensitive resource key.
         * @param culture Culture at which to begin lookup.
         * @return The bytes, or std::nullopt when @p name is absent or is not a binary resource.
         */
        [[nodiscard]] std::optional<std::vector<std::uint8_t>> GetByteArray(
            const std::string& name,
            const System::Globalization::CultureInfo& culture) const;

    private:
        /// The culture fallback both GetString and GetObject apply: the full culture, then each
        /// parent, then the invariant one. @p probe answers for one exact culture.
        template <typename TValue>
        [[nodiscard]] std::optional<TValue> WithCultureFallback(
            const std::string& startingCulture,
            const std::function<std::optional<TValue>(const std::string&)>& probe) const {
            std::string cultureName = startingCulture;
            for (;;) {
                if (std::optional<TValue> value = probe(cultureName)) {
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

        std::string baseName_;
        ResourceLookup lookup_;
        ObjectLookup objectLookup_;
    };

} // namespace System::Resources
