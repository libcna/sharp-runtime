// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Version.hpp"

namespace System {

    /**
     * @brief Provides a unique identity for a manifest-based application.
     *
     * C++ counterpart of .NET System.ApplicationId.
     * Stores the name, version, processor architecture, culture, and public key
     * token that together uniquely identify a ClickOnce application.
     *
     * @warning **This port's identity model and its text differ from .NET's, and
     * the divergences are under an open decision (#2291); do not read the
     * "counterpart of" lines below as parity.** Stated so a caller knows what is
     * actually promised, not to close SR-AUD-124 or SR-AUD-125 — nothing about
     * the surface changed when this note was written:
     *
     * - **The public key token is text, not bytes.** .NET takes a `byte[]` and
     *   clones it; this port takes a `std::string` and stores it verbatim. **No
     *   encoding is applied or assumed** — not hex, not base64 — so the string
     *   is whatever the caller passed, and a token containing arbitrary binary
     *   key material (embedded NULs included) cannot be round-tripped through
     *   this type.
     * - **There is no null/empty distinction.** .NET permits a null Culture and
     *   a null ProcessorArchitecture, distinguishable from explicitly empty
     *   ones. Here all three optional-ish components are mandatory
     *   `std::string`s, so **the empty string is the only representation of both
     *   "absent" and "empty"** and the two cannot be told apart afterwards.
     * - **The name is not validated.** .NET rejects a null or empty Name; this
     *   constructor accepts `""` and stores it.
     * - **`ToString()` is a different grammar and omits the token** — see its own
     *   comment.
     */
    class ApplicationId {
        std::string name_;
        Version     version_;
        std::string processorArchitecture_;
        std::string culture_;
        std::string publicKeyToken_;

    public:
        /**
         * @brief Initializes a new instance with the specified identity components.
         *
         * C++ counterpart of .NET ApplicationId(byte[], string, Version, string, string).
         * @param publicKeyToken The application's public key token (as a string).
         * @param name           The application name.
         * @param version        The application version.
         * @param processorArchitecture The processor architecture ("x86", "amd64", etc.).
         * @param culture        The culture string ("neutral", "en-US", etc.).
         */
        ApplicationId(const std::string& publicKeyToken,
                      const std::string& name,
                      const Version& version,
                      const std::string& processorArchitecture,
                      const std::string& culture)
            : name_(name), version_(version),
              processorArchitecture_(processorArchitecture),
              culture_(culture), publicKeyToken_(publicKeyToken) {}

        /** @brief Gets the application name. C++ counterpart of .NET ApplicationId.Name. */
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }

        /** @brief Gets the application version. C++ counterpart of .NET ApplicationId.Version. */
        [[nodiscard]] const Version& getVersionProperty() const { return version_; }

        /** @brief Gets the processor architecture. C++ counterpart of .NET ApplicationId.ProcessorArchitecture. */
        [[nodiscard]] const std::string& getProcessorArchitectureProperty() const {
            return processorArchitecture_;
        }

        /** @brief Gets the application culture. C++ counterpart of .NET ApplicationId.Culture. */
        [[nodiscard]] const std::string& getCultureProperty() const { return culture_; }

        /** @brief Gets the public key token. C++ counterpart of .NET ApplicationId.PublicKeyToken. */
        [[nodiscard]] const std::string& getPublicKeyTokenProperty() const { return publicKeyToken_; }

        /**
         * @brief Creates a copy of this ApplicationId.
         *
         * C++ counterpart of .NET ApplicationId.Copy().
         */
        [[nodiscard]] ApplicationId Copy() const { return *this; }

        /**
         * @brief Determines whether this instance and the specified object have the same value.
         *
         * C++ counterpart of .NET ApplicationId.Equals(object).
         * Two ApplicationId instances are equal when all five fields match.
         */
        [[nodiscard]] bool Equals(const ApplicationId& other) const {
            return name_                == other.name_
                && version_             == other.version_
                && processorArchitecture_ == other.processorArchitecture_
                && culture_             == other.culture_
                && publicKeyToken_      == other.publicKeyToken_;
        }

        bool operator==(const ApplicationId& o) const { return Equals(o); }
        bool operator!=(const ApplicationId& o) const { return !Equals(o); }

        /**
         * @brief Returns a hash code for this ApplicationId based on the name and version.
         *
         * C++ counterpart of .NET ApplicationId.GetHashCode(), which also derives
         * the code from those two components only. Equal instances therefore hash
         * equally even though `Equals` compares all five fields; unequal ones may
         * collide, which is permitted.
         *
         * @warning This is declared `noexcept` while `Version::ToString()` builds
         * a `std::string`, so an allocation failure here calls `std::terminate`
         * rather than propagating. Ticket #2292; no `SR-AUD-*` identifier.
         */
        [[nodiscard]] int GetHashCode() const noexcept {
            std::size_t h = std::hash<std::string>{}(name_);
            h ^= std::hash<std::string>{}(version_.ToString()) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return static_cast<int>(h);
        }

        /**
         * @brief Returns a string representation of the application identity,
         * in this port's own grammar.
         *
         * C++ counterpart of .NET ApplicationId.ToString() **in role only** — the
         * text is not .NET's. This emits
         * `<name>, Version=<v>, Culture=<c>, ProcessorArchitecture=<a>`:
         * capitalized unquoted keys, both optional components always present,
         * and **the public key token never included**. .NET writes lowercase
         * quoted `culture`, `version`, `publicKeyToken` (uppercase hex bytes) and
         * `processorArchitecture`, omitting components that are null.
         *
         * The consequence is worth stating plainly: **two ApplicationIds that
         * differ only by public key token produce identical text here**, so this
         * string does not identify an ApplicationId and must not be used as a
         * manifest identity or as an equality proxy. `Equals` compares all five
         * fields and does distinguish them.
         *
         * Changing this text would break any consumer that parses or logs it, and
         * matching .NET needs the byte-token representation SR-AUD-124 is about,
         * so both are held under one decision — SR-AUD-125, ticket #2291.
         */
        [[nodiscard]] std::string ToString() const {
            return name_ + ", Version=" + version_.ToString()
                + ", Culture=" + culture_
                + ", ProcessorArchitecture=" + processorArchitecture_;
        }
    };

} // namespace System
