// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>

namespace System {

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
     * Handles http, https, ftp, file, urn, and generic absolute URIs.
     * Parses scheme, user-info, host, port, path, query, and fragment components.
     *
     * Partial C++ counterpart of .NET System.Uri.
     *
     * @note Status: Partial — supports construction, component access, TryCreate,
     *   and relative-to-absolute combination. No percent-encoding/decoding.
     */
    class Uri {
        std::string absoluteUri_;
        std::string scheme_;
        std::string userInfo_;
        std::string host_;
        int         port_          = -1;
        std::string path_;
        std::string query_;    ///< includes leading '?'
        std::string fragment_; ///< includes leading '#'
        bool        isAbsoluteUri_ = true;

        /** Returns the well-known default port for the given scheme, or -1. */
        static int defaultPortForScheme(const std::string& scheme);

        /** Parses @p uriString into component fields. Called by all constructors. */
        void parse(const std::string& uriString);

    public:
        /**
         * @brief Constructs a Uri from an absolute or relative URI string.
         *
         * @throws std::invalid_argument if the string is empty or the scheme is malformed.
         */
        explicit Uri(const std::string& uriString);

        /**
         * @brief Constructs a Uri from a string, enforcing the specified kind.
         *
         * @throws std::invalid_argument if the URI does not match the requested kind.
         */
        Uri(const std::string& uriString, UriKind uriKind);

        /**
         * @brief Constructs an absolute URI by combining a base URI with a relative reference.
         *
         * If @p relativeUri already contains a scheme, it is parsed as an independent URI.
         *
         * @throws std::invalid_argument if @p baseUri is not absolute.
         */
        Uri(const Uri& baseUri, const std::string& relativeUri);

        /** @brief Returns the entire URI string (the value passed to the constructor). */
        [[nodiscard]] const std::string& getAbsoluteUriProperty()  const;

        /** @brief Returns the scheme (e.g. "https"), lower-case as parsed. */
        [[nodiscard]] const std::string& getSchemeProperty()        const;

        /** @brief Returns the host name or IP address (without port). */
        [[nodiscard]] const std::string& getHostProperty()          const;

        /** @brief Returns the port number, or the scheme default, or -1 if none. */
        [[nodiscard]] int                getPortProperty()           const;

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

        bool operator==(const Uri& other) const;
        bool operator!=(const Uri& other) const;

        /**
         * @brief Attempts to create a Uri from the given string and kind.
         *
         * @param uriString  The URI string to parse.
         * @param uriKind    The required kind.
         * @param result     Receives a shared_ptr to the new Uri on success, or nullptr.
         * @return @c true on success, @c false if the URI is invalid or the wrong kind.
         */
        static bool TryCreate(const std::string& uriString, UriKind uriKind,
                               std::shared_ptr<Uri>& result);
    };

} // namespace System
