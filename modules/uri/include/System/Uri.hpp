// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;

    /**
     * @brief Enumerates the kinds of URI a System::Uri may represent.
     *
     * Partial C++ counterpart of .NET System.UriKind.
     */
    enum class UriKind {
        RelativeOrAbsolute, ///< The URI kind is not determined.
        Absolute,           ///< The URI is an absolute URI.
        Relative            ///< The URI is a relative URI.
    };

    /**
     * @brief Provides an object representation of a uniform resource identifier (URI).
     *
     * Handles http, https, ftp, file, and generic hierarchical absolute URIs
     * (scheme "://" authority path), as well as opaque absolute URIs with no
     * authority (e.g. "mailto:user@example.com", "urn:isbn:0-395-36341-1").
     * Parses scheme, user-info, host, port, path, query, and fragment components.
     *
     * Partial C++ counterpart of .NET System.Uri.
     *
     * @note Status: Partial — supports construction, component access, TryCreate,
     *   and relative-to-absolute combination. No percent-encoding/decoding.
     * @note A hierarchical URI may have an **empty host** only when no port applies to it —
     *   neither an explicit port in the authority nor a default port for the scheme. That is
     *   what keeps `"file:///path"` legal while rejecting `"http:///path"` (ticket #2000).
     */
    class Uri {
        std::string absoluteUri_;
        std::string scheme_;
        std::string userInfo_;
        std::string host_;
        intcs       port_          = -1;
        std::string path_;
        std::string query_;    ///< includes leading '?'
        std::string fragment_; ///< includes leading '#'
        bool        isAbsoluteUri_ = true;

        /** Returns the well-known default port for the given scheme, or -1. */
        static intcs defaultPortForScheme(const std::string& scheme);

        /** Parses @p uriString into component fields. Called by all constructors. */
        void parse(const std::string& uriString);

    public:
        /**
         * @brief Constructs a Uri from an absolute or relative URI string.
         *
         * The string is absolute when it begins with an RFC 3986 scheme token followed by
         * ':'; it is hierarchical when "//" immediately follows that colon, and opaque
         * otherwise. Text whose leading token is not a scheme is a relative reference, not
         * an error — including text that merely *contains* "://" further along, such as
         * `"/path?redirect=http://example.com"` (ticket #1988).
         *
         * @throws System::UriFormatException if the string is empty, if the authority
         *         carries a malformed port or a malformed IP literal, or if the authority
         *         has **no host while a port applies** (ticket #2000). The last rule is
         *         stated in terms of the port, not the scheme: `"http://"`, `"http:///path"`,
         *         `"http://:80/path"` and `"http://user@/path"` are rejected because they
         *         would report the scheme's default port for a host that does not exist,
         *         while `"file:///path"` (RFC 8089) and every hierarchical scheme with no
         *         default-port entry keep an empty host with `Port == -1` and are accepted.
         */
        explicit Uri(const std::string& uriString);

        /**
         * @brief Constructs a Uri from a string, enforcing the specified kind.
         *
         * @throws System::ArgumentException (paramName @c "uriKind") if @p uriKind is not
         *         one of the three declared UriKind members. An out-of-domain cast used to
         *         fall through both guards and silently mean RelativeOrAbsolute
         *         (ticket #1992).
         * @throws System::UriFormatException if the URI does not match the requested kind.
         */
        Uri(const std::string& uriString, UriKind uriKind);

        /**
         * @brief Constructs an absolute URI by combining a base URI with a relative reference.
         *
         * If @p relativeUri already contains a scheme, it is parsed as an independent URI.
         *
         * @throws System::ArgumentOutOfRangeException if @p baseUri is not absolute.
         */
        Uri(const Uri& baseUri, const std::string& relativeUri);

        /** @brief Returns the entire URI string (the value passed to the constructor). */
        [[nodiscard]] const std::string& getAbsoluteUriProperty()  const;

        /**
         * @brief Returns a string representation of this Uri as originally supplied.
         *
         * C++ counterpart of .NET Uri.OriginalString -- was entirely missing from this port
         * despite being a commonly-used property (many call sites specifically want the
         * pristine original input rather than AbsoluteUri, since real .NET's AbsoluteUri
         * throws InvalidOperationException for a relative Uri while OriginalString never
         * throws). This implementation stores the raw constructor input verbatim as its
         * internal absoluteUri_ field with no canonicalization/re-escaping pass (a consequence
         * of this class's disclosed "no percent-encoding/decoding" limitation), so in this
         * port OriginalString and AbsoluteUri are the same underlying value -- unlike real
         * .NET, where AbsoluteUri is a canonicalized reconstruction (lower-cased scheme/host,
         * percent-encoded, default port stripped) that can differ from OriginalString.
         */
        [[nodiscard]] const std::string& getOriginalStringProperty() const { return absoluteUri_; }

        /**
         * @brief Returns the scheme (e.g. "https") exactly as it appeared in the input.
         *
         * This doc-comment used to promise the scheme "lower-case as parsed". The parser
         * has never lower-cased anything: `Uri("HTTP://EXAMPLE.COM/").getSchemeProperty()`
         * measurably returns `"HTTP"`, and `getHostProperty()` likewise returns
         * `"EXAMPLE.COM"`. Real .NET canonicalizes both. That divergence is a genuine open
         * finding (SR-AUD-142) whose repair changes equality and hash semantics and is
         * therefore approval-gated as ticket #1995; ticket #1994 corrected only the false
         * claim, so the documentation now matches the code rather than the intention.
         * Both behaviours are pinned by tests.
         */
        [[nodiscard]] const std::string& getSchemeProperty()        const;

        /** @brief Returns the host name or IP address (without port). */
        [[nodiscard]] const std::string& getHostProperty()          const;

        /** @brief Returns the port number, or the scheme default, or -1 if none. */
        [[nodiscard]] intcs               getPortProperty()           const;

        /** @brief Returns the absolute path component (starting with '/'). */
        [[nodiscard]] const std::string& getAbsolutePathProperty()  const;

        /** @brief Returns the query string including the leading '?', or empty. */
        [[nodiscard]] const std::string& getQueryProperty()         const;

        /** @brief Returns the fragment including the leading '#', or empty. */
        [[nodiscard]] const std::string& getFragmentProperty()      const;

        /** @brief Returns the user-info component (before '@'), or empty. */
        [[nodiscard]] const std::string& getUserInfoProperty()       const;

        /** @brief Returns @c true if this URI is absolute. */
        [[nodiscard]] bool               getIsAbsoluteUriProperty() const;

        /** @brief Returns the path and query concatenated. */
        [[nodiscard]] std::string getPathAndQueryProperty() const;

        /**
         * @brief Returns "host" or "host:port" depending on whether the port is
         *        the default for the scheme.
         */
        [[nodiscard]] std::string getAuthorityProperty() const;

        /** @brief Returns @c true if the host is localhost/127.0.0.1/::1. */
        [[nodiscard]] bool getIsLoopbackProperty() const;

        /** @brief Returns the original URI string. */
        [[nodiscard]] std::string ToString() const;

        /**
         * @brief Returns a hash code for this URI, consistent with operator==.
         * C++ counterpart of .NET Uri.GetHashCode().
         */
        [[nodiscard]] intcs GetHashCode() const;

        bool operator==(const Uri& other) const;
        bool operator!=(const Uri& other) const;

        /**
         * @brief Attempts to create a Uri from the given string and kind.
         *
         * @param uriString  The URI string to parse.
         * @param uriKind    The required kind.
         * @param result     Receives a shared_ptr to the new Uri on success, or nullptr.
         * @return @c true on success, @c false if the URI is invalid or the wrong kind.
         *
         * @note An out-of-domain @p uriKind makes this return @c false with a null result
         *   rather than propagating the ArgumentException the constructor throws, because
         *   this overload catches every exception. Whether .NET propagates instead could
         *   not be verified in this environment, so the current answer is pinned by a test
         *   rather than changed on recollection (ticket #1992, deferred verification).
         */
        static bool TryCreate(const std::string& uriString, UriKind uriKind,
                               std::shared_ptr<Uri>& result);

        /**
         * @brief Determines whether @p schemeName is a syntactically valid URI scheme.
         *
         * C++ counterpart of .NET `Uri.CheckSchemeName(string)` (`Uri.cs:1485-1488`),
         * transcribed exactly:
         *
         * ```csharp
         * !string.IsNullOrEmpty(schemeName) &&
         * char.IsAsciiLetter(schemeName[0]) &&
         * !schemeName.AsSpan().ContainsAnyExcept(s_schemeChars);
         * ```
         *
         * where `s_schemeChars` is `"+-." + digits + letters` (`Uri.cs:1477-1478`), i.e. RFC
         * 3986 §3.1's `ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )`.
         *
         * Added by ticket #1998, which needed it: `UriParser::IsKnownScheme` must **reject** a
         * malformed scheme rather than report it as merely unknown, and .NET expresses that by
         * calling this. It is public here because it is public in .NET.
         *
         * @note Case-insensitive by construction — both cases are in the allowed set. `net.tcp`
         *       and `net.pipe`, two of the schemes this port registers, rely on `.` being
         *       allowed.
         */
        [[nodiscard]] static bool CheckSchemeName(const std::string& schemeName) noexcept {
            if (schemeName.empty()) return false;
            const unsigned char first = static_cast<unsigned char>(schemeName[0]);
            const bool firstIsLetter =
                (first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z');
            if (!firstIsLetter) return false;
            for (char raw : schemeName) {
                const unsigned char c = static_cast<unsigned char>(raw);
                const bool allowed = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                     (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
                if (!allowed) return false;
            }
            return true;
        }
    };

} // namespace System
