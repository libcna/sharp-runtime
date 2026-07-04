// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include "System/NotImplementedException.hpp"
#include "System/UriComponents.hpp"
#include "System/UriFormat.hpp"
#include "System/UriFormatException.hpp"

namespace System {

    class Uri; // forward declaration

    /**
     * @brief Parses a new URI scheme.
     *
     * C++ counterpart of .NET System.UriParser (abstract).
     * Provides the extensibility point for registering custom URI schemes.
     * All methods are stubs — custom scheme registration is not supported.
     */
    class UriParser {
    protected:
        /** @brief Initializes a new instance of the UriParser class. */
        UriParser() = default;

    public:
        /** @brief Virtual destructor. */
        virtual ~UriParser() = default;

        /**
         * @brief Returns the components of a URI.
         *
         * C++ counterpart of .NET UriParser.GetComponents(Uri, UriComponents, UriFormat).
         * @throws NotImplementedException Always — override in a subclass.
         */
        virtual std::string GetComponents(const Uri& /*uri*/,
                                          UriComponents /*components*/,
                                          UriFormat /*format*/) {
            throw NotImplementedException("UriParser.GetComponents: not implemented.");
        }

        /**
         * @brief Returns true if the base URI is the base of the relative URI.
         *
         * C++ counterpart of .NET UriParser.IsBaseOf(Uri, Uri).
         */
        virtual bool IsBaseOf(const Uri& /*baseUri*/, const Uri& /*relativeUri*/) {
            return false;
        }

        /**
         * @brief Indicates whether the original URI string was well-formed.
         *
         * C++ counterpart of .NET UriParser.IsWellFormedOriginalString(Uri).
         */
        virtual bool IsWellFormedOriginalString(const Uri& /*uri*/) {
            return true;
        }

        /**
         * @brief Returns true if the scheme name is registered.
         *
         * C++ counterpart of .NET UriParser.IsKnownScheme(string).
         * Recognises the common built-in schemes (http, https, ftp, file, mailto, etc.).
         * @param schemeName The scheme name to check (case-insensitive).
         */
        static bool IsKnownScheme(const std::string& schemeName) {
            static const char* known[] = {
                "http", "https", "ftp", "file", "mailto",
                "gopher", "news", "nntp", "telnet", "wais",
                "ldap", "net.pipe", "net.tcp"
            };
            std::string lower = schemeName;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            for (const auto* s : known) {
                if (lower == s) return true;
            }
            return false;
        }
    };

} // namespace System
