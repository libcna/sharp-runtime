// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <optional>
#include <vector>
#include <utility>
#include "System/ArgumentException.hpp"
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
        std::string                name_;
        Version                    version_;
        std::optional<std::string> processorArchitecture_;
        std::optional<std::string> culture_;
        std::vector<SharpRuntime::bytecs> publicKeyToken_;

    public:
        /**
         * @brief Constructs an ApplicationId.
         *
         * C++ counterpart of .NET `ApplicationId(byte[], string, Version, string?, string?)`
         * (`ApplicationId.cs:13-24`).
         *
         * @par Ticket #2291 took all four of the review's decisions, toward .NET
         *  1. **The name is validated.** .NET raises for a null or empty name; this port accepted
         *     `""` silently. `ArgumentException::ThrowIfNullOrEmpty` is the port's existing
         *     helper, so no message is invented.
         *  2. **The token is bytes, not text.** It was a `std::string` stored verbatim, so binary
         *     key material was unrepresentable and no clone was made. .NET takes `byte[]` and
         *     **clones on the way in and on the way out**, so a caller cannot mutate a stored
         *     token through the array it passed or the one it received.
         *  3. **Culture and ProcessorArchitecture are optional.** They are `string?` in .NET, and
         *     were non-nullable `std::string` here, so absent and empty were one state.
         *  4. **`ToString()` adopts .NET's grammar** — see its own doc-comment.
         *
         * @throws System::ArgumentException if @p name is empty.
         */
        ApplicationId(std::vector<SharpRuntime::bytecs> publicKeyToken,
                      const std::string& name,
                      const Version& version,
                      std::optional<std::string> processorArchitecture,
                      std::optional<std::string> culture)
            : name_(name), version_(version),
              processorArchitecture_(std::move(processorArchitecture)),
              culture_(std::move(culture)),
              publicKeyToken_(std::move(publicKeyToken)) {
            System::ArgumentException::ThrowIfNullOrEmpty(name, "name");
        }

        /** @brief Gets the application name. C++ counterpart of .NET ApplicationId.Name. */
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }

        /** @brief Gets the application version. C++ counterpart of .NET ApplicationId.Version. */
        [[nodiscard]] const Version& getVersionProperty() const { return version_; }

        /**
         * @brief Gets the processor architecture, or `std::nullopt` if absent.
         *
         * C++ counterpart of .NET `ApplicationId.ProcessorArchitecture`, which is `string?`.
         * Nullable since ticket #2291.
         */
        [[nodiscard]] const std::optional<std::string>& getProcessorArchitectureProperty() const {
            return processorArchitecture_;
        }

        /**
         * @brief Gets the application culture, or `std::nullopt` if absent.
         *
         * C++ counterpart of .NET `ApplicationId.Culture`, which is `string?`.
         * Nullable since ticket #2291.
         */
        [[nodiscard]] const std::optional<std::string>& getCultureProperty() const {
            return culture_;
        }

        /**
         * @brief Gets a COPY of the public key token.
         *
         * C++ counterpart of .NET `ApplicationId.PublicKeyToken`, which is
         * `=> (byte[])_publicKeyToken.Clone()` (`ApplicationId.cs:34`) — a defensive copy on
         * **every access**. Returning by value is that clone; a `const&` would have handed the
         * caller the stored array and defeated the constructor's own copy.
         */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> getPublicKeyTokenProperty() const {
            return publicKeyToken_;
        }

        /**
         * @brief Creates a copy of this ApplicationId.
         *
         * C++ counterpart of .NET ApplicationId.Copy().
         */
        [[nodiscard]] ApplicationId Copy() const { return *this; }

        /**
         * @brief Determines whether this instance and the specified object have the same value.
         *
         * C++ counterpart of .NET `ApplicationId.Equals` (`ApplicationId.cs:71-77`), which
         * compares all five components and the token **element by element**.
         */
        [[nodiscard]] bool Equals(const ApplicationId& other) const {
            return name_                  == other.name_
                && version_               == other.version_
                && processorArchitecture_ == other.processorArchitecture_
                && culture_               == other.culture_
                && publicKeyToken_        == other.publicKeyToken_;
        }

        bool operator==(const ApplicationId& o) const { return Equals(o); }
        bool operator!=(const ApplicationId& o) const { return !Equals(o); }

        /**
         * @brief Returns a hash code derived from the name and version only.
         *
         * C++ counterpart of .NET `ApplicationId.GetHashCode()`
         * (`ApplicationId.cs:79-82`), which carries its own comment: *"purposely skipping
         * publicKeyToken, processor architecture and culture as they are less likely to make
         * things not equal than name and version."* Equal instances therefore hash equally even
         * though `Equals` compares all five; unequal ones may collide, which is permitted.
         *
         * @note It is **no longer `noexcept`**, and that closes ticket #2292: it used to hash
         *       `version_.ToString()`, which allocates, so an allocation failure called
         *       `std::terminate` rather than propagating. It now composes `Version::GetHashCode()`
         *       instead, which is both what .NET does and allocation-free — so the `noexcept`
         *       could arguably have stayed, and is dropped anyway because a hash that composes
         *       another type's virtual-free but user-defined hash should not promise more than
         *       that hash does.
         */
        [[nodiscard]] SharpRuntime::intcs GetHashCode() const {
            return static_cast<SharpRuntime::intcs>(
                static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(name_)) ^
                version_.GetHashCode());
        }

        /**
         * @brief Returns .NET's textual representation of the application identity.
         *
         * C++ counterpart of .NET `ApplicationId.ToString()` (`ApplicationId.cs:38-69`),
         * transcribed since ticket #2291. The grammar is:
         *
         *     <name>[, culture="<c>"], version="<v>"[, publicKeyToken="<HEX>"][, processorArchitecture ="<a>"]
         *
         * lowercase quoted keys, absent components omitted, and the token as **uppercase** hex.
         *
         * @note Two details are transcribed rather than tidied, because they are the reference's
         *       and a caller may match on them. The token is emitted even when EMPTY -- .NET's
         *       guard is `_publicKeyToken != null`, and a zero-length array is not null, so an
         *       empty token yields `publicKeyToken=""`. And `processorArchitecture` carries a
         *       **space before its `=`**, which the other three keys do not; that asymmetry is in
         *       `ApplicationId.cs:63` and is reproduced deliberately.
         *
         * Before #2291 this emitted a different grammar of its own -- capitalized unquoted keys,
         * both optional components always present, and **the token never included**, so two
         * identities differing only by token produced identical text.
         */
        [[nodiscard]] std::string ToString() const {
            static constexpr char kHexDigits[] = "0123456789ABCDEF";
            std::string out = name_;
            if (culture_.has_value()) {
                out += ", culture=\"";
                out += *culture_;
                out += '"';
            }
            out += ", version=\"";
            out += version_.ToString();
            out += '"';
            // Always emitted: .NET's guard is a null test on the array, and this port's
            // std::vector is never null -- so an empty token prints as an empty quoted value,
            // which is what .NET does for `new byte[0]` too.
            out += ", publicKeyToken=\"";
            for (SharpRuntime::bytecs b : publicKeyToken_) {
                const auto v = static_cast<unsigned char>(b);
                out += kHexDigits[(v >> 4) & 0x0F];
                out += kHexDigits[v & 0x0F];
            }
            out += '"';
            if (processorArchitecture_.has_value()) {
                out += ", processorArchitecture =\"";   // the reference's own space before '='
                out += *processorArchitecture_;
                out += '"';
            }
            return out;
        }
    };

} // namespace System
