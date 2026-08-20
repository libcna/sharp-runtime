// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/NotImplementedException.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Uri.hpp"
#include "System/UriComponents.hpp"
#include "System/UriFormat.hpp"
#include "System/UriFormatException.hpp"

namespace System {

    using SharpRuntime::intcs;

    class Uri; // forward declaration

    /**
     * @brief Parses a new URI scheme.
     *
     * C++ counterpart of .NET `System.UriParser` (`abstract`).
     *
     * @note **THE OVERRIDE HOOKS BELOW ARE `protected` AND THAT IS THE CONTRACT, NOT A DETAIL**
     *       (#1997 group A-4, SR-AUD-146). They exist to be **overridden**, never to be **called**
     *       by a third party -- .NET says so in a comment of its own, describing its internal
     *       forwarders as existing *"to avoid `protected internal` signatures in the public
     *       docs"* (`UriSyntax.cs:245-246`). This port published three of them as **public**, so
     *       any caller holding a `UriParser&` could invoke another parser's hook directly. A
     *       subclass that wants its own hook reachable from outside publishes a forwarder of its
     *       own, which is exactly what .NET's `InternalGetComponents` and friends are.
     *
     * @note **NOTHING IN THIS RUNTIME CALLS THE OVERRIDE HOOKS**, and that is declared here rather
     *       than left to be discovered. This port's `System::Uri` performs its own parse and never
     *       consults a `UriParser`, so `GetComponents`, `IsBaseOf` and
     *       `IsWellFormedOriginalString` are override points with no caller. `OnRegister` is the
     *       one exception and the only one: `Register` calls it, so a subclass really can observe
     *       its own registration.
     */
    class UriParser {
    protected:
        /**
         * @brief Initializes a new instance of the UriParser class.
         *
         * C++ counterpart of .NET's `protected UriParser()` (`UriScheme.cs:41`).
         */
        UriParser() = default;

        /**
         * @brief Called when this parser is registered against a scheme.
         *
         * C++ counterpart of .NET `UriParser.OnRegister(string, int)` (`UriScheme.cs:56-58`),
         * whose base implementation is empty and is transcribed as such.
         *
         * @note **THE SCHEME ARRIVES AS A PARAMETER BECAUSE THE PARSER DOES NOT YET KNOW IT.**
         *       .NET assigns `syntax._scheme` on the line *after* the `OnRegister` call
         *       (`UriSyntax.cs:175-176`), so during the callback `getSchemeNameProperty()` is
         *       still empty -- which is what makes the parameter load-bearing rather than a
         *       convenience. This port keeps that order, and it is pinned.
         *
         * @param schemeName The lower-cased scheme this parser is being registered for.
         * @param defaultPort The scheme's default port, or -1 for none.
         */
        virtual void OnRegister(const std::string& /*schemeName*/, intcs /*defaultPort*/) {}

        /**
         * @brief Returns the components of a URI.
         *
         * C++ counterpart of .NET `UriParser.GetComponents(Uri, UriComponents, UriFormat)`.
         * @throws NotImplementedException Always -- override in a subclass.
         */
        virtual std::string GetComponents(const Uri& /*uri*/,
                                          UriComponents /*components*/,
                                          UriFormat /*format*/) {
            throw NotImplementedException("UriParser.GetComponents: not implemented.");
        }

        /**
         * @brief Returns true if the base URI is the base of the relative URI.
         *
         * C++ counterpart of .NET `UriParser.IsBaseOf(Uri, Uri)`.
         */
        virtual bool IsBaseOf(const Uri& /*baseUri*/, const Uri& /*relativeUri*/) {
            return false;
        }

        /**
         * @brief Indicates whether the original URI string was well-formed.
         *
         * C++ counterpart of .NET `UriParser.IsWellFormedOriginalString(Uri)`.
         */
        virtual bool IsWellFormedOriginalString(const Uri& /*uri*/) {
            return true;
        }

        /**
         * @brief The scheme this parser was registered for, or the empty string.
         *
         * C++ counterpart of .NET's `internal string SchemeName` (`UriScheme.cs:18`). It is
         * `protected` rather than `public` here because .NET does not publish it and this port
         * has a real creator for it -- `Register`, a static member of this very class -- so
         * **SA-12's first branch applies** and no accessibility divergence needs recording.
         */
        [[nodiscard]] const std::string& getSchemeNameProperty() const noexcept { return scheme_; }

        /** @brief The default port this parser was registered with, or -1. */
        [[nodiscard]] intcs getDefaultPortProperty() const noexcept { return port_; }

    public:
        /** @brief Virtual destructor. */
        virtual ~UriParser() = default;

        /**
         * @brief Registers a custom URI parser against a scheme name.
         *
         * C++ counterpart of .NET `UriParser.Register(UriParser, string, int)`
         * (`UriScheme.cs:166-181`).
         *
         * @note **THE REGISTRATION IS OBSERVABLE, WHICH IS THE WHOLE POINT.** A `Register` that
         *       validated its arguments and stored into a table nothing reads would be accepted
         *       and ignored -- the SR-AUD-168 defect. After a successful call
         *       `IsKnownScheme(schemeName)` answers **true**, which is .NET's own linkage:
         *       `Register` reaches `s_table` and `IsKnownScheme` reads it through `GetSyntax`.
         *
         * @note **A `std::shared_ptr` RATHER THAN A REFERENCE**, because .NET's static table holds
         *       a strong reference for the life of the process and entries are never removed. A
         *       raw pointer would leave the registry holding something a caller may destroy, and a
         *       reference could not express the null argument .NET rejects.
         *
         * @param uriParser The parser to register. Must not be null and must not already be
         *                  registered against a scheme.
         * @param schemeName The scheme name; lower-cased before registration.
         * @param defaultPort The scheme's default port (0..65535), or -1 for none.
         * @throws System::ArgumentNullException if @p uriParser is null.
         * @throws System::ArgumentOutOfRangeException if @p schemeName is not a valid scheme name,
         *         if it is exactly one character long, or if @p defaultPort is out of range.
         * @throws System::InvalidOperationException if @p uriParser is already registered, or if
         *         @p schemeName already has a registered parser.
         */
        static void Register(const std::shared_ptr<UriParser>& uriParser,
                             const std::string& schemeName, intcs defaultPort);

        /**
         * @brief Returns true if the scheme name is registered.
         *
         * C++ counterpart of .NET `UriParser.IsKnownScheme(string)`. Recognises .NET's built-in
         * table and any scheme a caller has registered through Register().
         *
         * @param schemeName The scheme name to check (case-insensitive).
         * @throws System::ArgumentOutOfRangeException if @p schemeName is not a valid scheme name.
         */
        [[nodiscard]] static bool IsKnownScheme(const std::string& schemeName);

    private:
        std::string scheme_;
        intcs       port_ = -1;
    };

} // namespace System
