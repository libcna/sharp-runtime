// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <optional>
#include <string>
#include <utility>
#include "System/Attribute.hpp"

namespace System::Runtime::Versioning {

    /** Identifies the version of the .NET framework targeted by the assembly. */
    class TargetFrameworkAttribute : public System::Attribute {
        std::string frameworkName_;
        std::optional<std::string> frameworkDisplayName_;
    public:
        /** @param frameworkName The framework moniker (e.g. ".NETCoreApp,Version=v8.0"). */
        explicit TargetFrameworkAttribute(const std::string& frameworkName)
            : frameworkName_(frameworkName) {}

        /** @return The framework moniker string. */
        [[nodiscard]] const std::string& getFrameworkNameProperty()        const { return frameworkName_; }

        /** @return The human-readable framework display name, or nullopt if unset. */
        [[nodiscard]] const std::optional<std::string>& getFrameworkDisplayNameProperty() const {
            return frameworkDisplayName_;
        }

        /** Sets or clears the nullable human-readable framework display name. */
        void setFrameworkDisplayNameProperty(std::optional<std::string> value) {
            frameworkDisplayName_ = std::move(value);
        }
    };

    /** Indicates that an API is supported on the specified OS platform. */
    /**
     * @brief The base every platform-name attribute shares. `PlatformAttributes.cs:1-18`.
     *
     * C++ counterpart of .NET `System.Runtime.Versioning.OSPlatformAttribute`, which is
     * `abstract class OSPlatformAttribute : Attribute` with a `private protected` constructor and
     * a get-only `public string PlatformName`.
     *
     * @note **Introduced by #1980 G-3 under SA-15.3**, the approval that lifted SA-3's exclusion of
     * base-class changes. Before it, the five platform attributes each derived from
     * `System::Attribute` directly and **each carried its own copy of `platformName_` and its own
     * `getPlatformNameProperty()`** — five duplicates of one fact, and no type through which a
     * caller could handle "any platform attribute" at all, which is what SR-AUD-163 named.
     *
     * @note **`protected`, not `private protected`.** C++ has no equivalent of C#'s
     * `private protected` (accessible to derived classes *in the same assembly only*), and this
     * port has no assembly boundary to express the second half of it. `protected` keeps the class
     * unconstructible from outside the hierarchy, which is the part that carries meaning here; the
     * assembly restriction is not expressible and is not pretended.
     *
     * @note **`TargetPlatformAttribute` is .NET's sixth derived type and is absent here**, and that
     * is stated so a later reader does not mistake five for the whole set. Adding it is additive
     * and outside G-3, whose wording is "introduce `OSPlatformAttribute` and reparent five
     * attributes".
     */
    class OSPlatformAttribute : public System::Attribute {
        std::string platformName_;

    protected:
        explicit OSPlatformAttribute(const std::string& platformName)
            : platformName_(platformName) {}

    public:
        /** @return The platform name this attribute names. `PlatformAttributes.cs:17`. */
        [[nodiscard]] const std::string& getPlatformNameProperty() const { return platformName_; }
    };

    class SupportedOSPlatformAttribute final : public OSPlatformAttribute {
    public:
        /** @param platformName Platform identifier (e.g. "windows", "linux10.0"). */
        explicit SupportedOSPlatformAttribute(const std::string& platformName)
            : OSPlatformAttribute(platformName) {}

        /** @return The platform identifier. */
    };

    /** Indicates that an API is not supported on the specified OS platform. */
    class UnsupportedOSPlatformAttribute final : public OSPlatformAttribute {
        std::optional<std::string> message_;
    public:
        /** @param platformName Platform identifier (e.g. "windows"). */
        explicit UnsupportedOSPlatformAttribute(const std::string& platformName)
            : OSPlatformAttribute(platformName) {}

        /**
         * @param platformName Platform identifier (e.g. "windows").
         * @param message      Optional message explaining the lack of support.
         */
        UnsupportedOSPlatformAttribute(const std::string& platformName,
                                       std::optional<std::string> message)
            : OSPlatformAttribute(platformName), message_(std::move(message)) {}

        /** @return The platform identifier. */

        /** @return The explanatory message, or nullopt if not provided. */
        [[nodiscard]] const std::optional<std::string>& getMessageProperty() const {
            return message_;
        }
    };

    /**
     * Annotates a custom guard field, property, or method with a supported platform name, for use
     * in conditionals/asserts that guard calls to platform-specific APIs.
     */
    class SupportedOSPlatformGuardAttribute final : public OSPlatformAttribute {
    public:
        /** @param platformName Platform identifier the guard indicates support for. */
        explicit SupportedOSPlatformGuardAttribute(const std::string& platformName)
            : OSPlatformAttribute(platformName) {}

        /** @return The platform identifier. */
    };

    /**
     * Annotates a custom guard field, property, or method with an unsupported platform name, for
     * use in conditionals/asserts that guard against calling unsupported platform-specific APIs.
     */
    class UnsupportedOSPlatformGuardAttribute final : public OSPlatformAttribute {
    public:
        /** @param platformName Platform identifier the guard indicates lack of support for. */
        explicit UnsupportedOSPlatformGuardAttribute(const std::string& platformName)
            : OSPlatformAttribute(platformName) {}

        /** @return The platform identifier. */
    };

    /** Indicates that an API has been obsoleted on the specified OS platform. */
    class ObsoletedOSPlatformAttribute final : public OSPlatformAttribute {
        std::optional<std::string> message_;
        std::optional<std::string> url_;
    public:
        /**
         * @param platformName Platform on which the API is obsolete.
         * @param message      Optional deprecation message.
         *
         * #1980 group G-4 / SR-AUD-164. **There is no `url` parameter, deliberately.** .NET
         * declares exactly two constructors -- `(platformName)` and `(platformName, message)`
         * (`PlatformAttributes.cs`) -- and exposes the URL as a **settable property**,
         * `public string? Url { get; set; }`. This port had it the other way round: a third
         * constructor parameter .NET does not have, feeding a read-only accessor. Both halves
         * were wrong, and in opposite directions.
         */
        explicit ObsoletedOSPlatformAttribute(const std::string& platformName,
                                               std::optional<std::string> message = std::nullopt)
            : OSPlatformAttribute(platformName), message_(std::move(message)) {}

        /** @return The platform identifier. */

        /** @return The deprecation message, or nullopt if not provided. */
        [[nodiscard]] const std::optional<std::string>& getMessageProperty() const {
            return message_;
        }

        /** @return The migration URL, or nullopt if not set. */
        [[nodiscard]] const std::optional<std::string>& getUrlProperty() const { return url_; }

        /**
         * @brief Sets the URL that provides more information about the obsolescence.
         *
         * #1980 G-4 / SR-AUD-164: .NET's `Url` is `{ get; set; }` -- a fully settable property,
         * not a constructor parameter.
         */
        void setUrlProperty(std::optional<std::string> value) { url_ = std::move(value); }
    };

    /** Marks an API as requiring preview features that may change in future releases. */
    class RequiresPreviewFeaturesAttribute : public System::Attribute {
        std::optional<std::string> message_;
        std::optional<std::string> url_;
    public:
        /** Default constructor — no message or URL. */
        RequiresPreviewFeaturesAttribute() = default;

        /**
         * @param message Explanation of why the API is preview.
         *
         * #1980 group G-4 / SR-AUD-164. **No `url` parameter**, matching .NET's
         * `public RequiresPreviewFeaturesAttribute(string? message)`
         * (`RequiresPreviewFeaturesAttribute.cs:34`); the URL is a settable property there.
         */
        explicit RequiresPreviewFeaturesAttribute(std::optional<std::string> message)
            : message_(std::move(message)) {}

        /** @return The informational message, or nullopt if not provided. */
        [[nodiscard]] const std::optional<std::string>& getMessageProperty() const {
            return message_;
        }

        /** @return The informational URL, or nullopt if not set. */
        [[nodiscard]] const std::optional<std::string>& getUrlProperty() const { return url_; }

        /**
         * @brief Sets the URL that provides more information.
         *
         * #1980 G-4 / SR-AUD-164: .NET's `Url` is `{ get; set; }`
         * (`RequiresPreviewFeaturesAttribute.cs:47`).
         */
        void setUrlProperty(std::optional<std::string> value) { url_ = std::move(value); }
    };

} // namespace System::Runtime::Versioning
